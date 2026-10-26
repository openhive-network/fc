#!/usr/bin/env bash
# Run the checks named after the suite and write test-results/<suite>/junit.xml.
#
#   .aidev/run-checks.sh <suite> <step>...
#
# Steps, one junit case each (a test step adds one case per test binary):
#
#   build   cmake configure (Release, FC_BUILD_TESTS=ON) and build, as CI's
#           build:linux job; the build tree is .aidev/build/ (ignored)
#   unit    the test binaries CI's `test` job runs, with the same arguments;
#           the Boost.Test ones also keep Boost's own junit as <name>.boost.xml
#   all     the all_tests Boost.Test binary (not run by CI), without the
#           cases that need the network or already fail (ALL_NEEDS_NETWORK,
#           ALL_KNOWN_FAILING below)
#
# Runs offline in the image the profile pins (common-ci-configuration's
# ci-base-image-ubuntu, the image of CI's build:linux and test jobs). Needs the
# submodules vendor/secp256k1-zkp and vendor/websocketpp checked out.
set -uo pipefail
cd "$(dirname "$0")/.." || exit 1
root="$PWD"
suite="${1:?usage: $0 <suite> <step>...}"; shift
out="$root/test-results/$suite"
build="$root/.aidev/build"
rm -rf "$out"; mkdir -p "$out"
cases=()
status=0

xml_escape() { sed -e 's/&/\&amp;/g' -e 's/</\&lt;/g' -e 's/>/\&gt;/g' -e 's/"/\&quot;/g' -e 's/[\x00-\x08\x0b\x0c\x0e-\x1f]//g'; }

# record NAME pass|fail SECONDS LOG
record() {
    local name="$1" result="$2" secs="$3" log="$4" body
    if [ "$result" = pass ]; then
        cases+=("<testcase classname=\"fc.$suite\" name=\"$name\" time=\"$secs\"></testcase>")
    else
        body="$(tail -n 80 "$log" | xml_escape)"
        cases+=("<testcase classname=\"fc.$suite\" name=\"$name\" time=\"$secs\"><failure message=\"$name failed\">$body</failure></testcase>")
        status=1
    fi
}

# run NAME CMD... — run CMD with its output shown and logged, record a case
run() {
    local name="$1"; shift
    local log="$out/$name.log" start rc
    start=$SECONDS
    echo "=== $name: $* ==="
    (cd "${RUN_DIR:-$root}" && "$@") > >(tee "$log") 2>&1; rc=$?
    record "$name" "$([ "$rc" -eq 0 ] && echo pass || echo fail)" "$((SECONDS - start))" "$log"
    return "$rc"
}

# The all_tests cases that need the network: ntp_test queries pool.ntp.org,
# websocket_test opens sockets to its own server and resolves names.
ALL_NEEDS_NETWORK=(fc_network/ntp_test fc_network/websocket_test)
# The all_tests cases that fail on main at onboarding (2026-10-26, 7951549),
# offline or not; see .aidev/README.md. Remove a line once its case passes.
#   fc/real128_test        real128 arithmetic and to-string give wrong values
#   ip_tests/legacy_*      the legacy IPv4 types accept IPv6 input
#   fc_crypto/rand_test    a statistical bound on rand_bytes, outside it here
ALL_KNOWN_FAILING=(fc/real128_test ip_tests/legacy_address_rejects_ipv6
                   ip_tests/legacy_endpoint_rejects_ipv6 fc_crypto/rand_test)

built=0
for step in "$@"; do
    case "$step" in
        build)
            if [ ! -f vendor/secp256k1-zkp/include/secp256k1.h ] || [ ! -f vendor/websocketpp/websocketpp/version.hpp ]; then
                echo "submodules missing: vendor/secp256k1-zkp and vendor/websocketpp must be checked out" > "$out/build.log"
                record build fail 0 "$out/build.log"
                break
            fi
            run configure cmake -S "$root" -B "$build" -DCMAKE_BUILD_TYPE=Release -DFC_BUILD_TESTS=ON \
                && run build cmake --build "$build" -j"$(nproc)" && built=1
            [ "$built" -eq 1 ] || break
            ;;
        unit)
            t="$build/tests"
            export RUN_DIR="$out"   # the unit binaries need no tree files
            run variant_test "$t/variant_test" --log_level=test_suite --logger="JUNIT,all,$out/variant_test.boost.xml" --logger=HRF,test_suite
            run locale_test "$t/locale_test" --log_level=test_suite --logger="JUNIT,all,$out/locale_test.boost.xml" --logger=HRF,test_suite
            run sha_test "$t/sha_test"
            run hmac_test "$t/hmac_test" --logger="JUNIT,all,$out/hmac_test.boost.xml" --logger=HRF,error
            run saturation_test "$t/saturation_test"
            run ecdsa_canon_test "$t/ecdsa_canon_test"
            run ecc_test "$t/ecc_test" testpassword "$out/ecc_interop.dat"
            ;;
        all)
            unset RUN_DIR
            skip=(); for c in "${ALL_NEEDS_NETWORK[@]}" "${ALL_KNOWN_FAILING[@]}"; do skip+=(--run_test="!$c"); done
            run all_tests timeout 900 "$build/tests/all_tests" "${skip[@]}" --log_level=test_suite \
                --logger="JUNIT,all,$out/all_tests.boost.xml" --logger=HRF,test_suite
            # bloom_test reads README.md from the working directory and writes
            # words.txt beside it: run from the root, keep words.txt with the results.
            mv -f "$root/words.txt" "$out/" 2>/dev/null
            ;;
        *)
            echo "unknown step: $step" > "$out/$step.log"
            record "$step" fail 0 "$out/$step.log"
            ;;
    esac
done

failures=0
for c in "${cases[@]}"; do case "$c" in *"<failure"*) failures=$((failures + 1));; esac; done
{
    echo '<?xml version="1.0" encoding="UTF-8"?>'
    echo "<testsuite name=\"fc.$suite\" tests=\"${#cases[@]}\" failures=\"$failures\" errors=\"0\" skipped=\"0\">"
    printf '%s\n' "${cases[@]}"
    echo '</testsuite>'
} > "$out/junit.xml"
echo "junit: $out/junit.xml (${#cases[@]} cases, $failures failed)"
exit "$status"
