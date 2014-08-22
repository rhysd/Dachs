#!/usr/bin/env sh

GREEN=`echo '\033[92m'`
RED=`echo '\033[91m'`
YELLOW=`echo '\033[93m'`
BLUE=`echo '\033[94m'`
RESET=`echo '\033[0m'`

start="s/^Start testing:/$YELLOW&$RESET/g;"
end="s/^End testing:/$YELLOW&$RESET/g;"
passed="s/^Test Passed\\.$/$GREEN&$RESET/g;"
failed="s/^Test Failed\\.$/$RED&$RESET/g;"
error="s/^[/._:[:alnum:]]+ error in \".+\":.*$/$RED&$RESET/g;"
error2="s/fatal error in \".+\": .*$/$RED&$RESET/g;"
info="s/\\*\\*\\*/$BLUE&$RESET/g;"

exec cat "$(pwd)/Testing/Temporary/LastTest.log" 2>&1 |\
    sed -E "\
        $start\
        $end\
        $passed\
        $failed\
        $error\
        $error2\
        $info
        "
