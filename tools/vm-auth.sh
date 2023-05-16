#!/bin/bash
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

fifo=fifo

rm ${fifo}
mkfifo ${fifo}

exec 3<> ${fifo}

question="Are you sure you want to continue connecting?"
answer="yes"

question2="password:"
question3="Password:"
answer2="ubuntu"

while IFS= read -d $'\0' -n 1 a ; do
    str+="${a}"

    if [ "${str}" = "${question}" ] ; then
        echo "!!! found: ${str}"
        echo ">>> sending answer: ${answer}"
        echo "${answer}" >&3
        unset str
    fi

    if [ "${str}" = "${question2}" ] ; then
        echo "!!! found: ${str}"
        echo ">>> sending answer: ${answer2}"
        echo "${answer2}" >&3
        unset str
    fi

    if [ "${str}" = "${question3}" ] ; then
        echo "!!! found: ${str}"
        echo ">>> sending answer: ${answer2}"
        echo "${answer2}" >&3
        unset str
    fi

    if [ "$a" = $'\n' ] ; then
        echo -n "--- discarding input line: ${str}"
        unset str
    fi

done < <($SCRIPT_DIR/buildbot-vm.sh setup-ssh <${fifo})