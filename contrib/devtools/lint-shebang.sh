#!/usr/bin/env bash

export LC_ALL=C

cd $(git rev-parse --show-toplevel)
python_exclusions="test_framework.*"
bash_exclusions="src/(univalue|secp256k1)/.*"

python_files=$(git ls-files "test/functional/*.py" | grep -v -E "${python_exclusions}")
bash_files=$(git ls-files "*.sh" | grep -v -E "${bash_exclusions}")

RET=0
for file in ${python_files}
do
    line=$(head -n 1 ${file})
    if [[ ${line} != "#!/usr/bin/env python3" ]]; then
        echo "Missing python shebang in ${file}"
        RET=1;
    fi
done;

for file in ${bash_files}
do
    line=$(head -n 1 ${file})
    if [[ ${line} != "#!/usr/bin/env bash" ]]; then
        echo "Wrong bash shebang in ${file}: : ${line}"
        RET=1;
    fi
done;

exit ${RET}
