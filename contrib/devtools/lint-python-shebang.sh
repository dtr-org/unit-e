#!/usr/bin/env bash

export LC_ALL=C

cd $(git rev-parse --show-toplevel)/test/functional
exclusions="test_framework.*"

files=$(git ls-files | grep -E ".*\.py" | grep -v -E ${exclusions})

RET=0
for file in ${files}
do
    line=$(head -n 1 ${file})
    if [[ ${line} != "#!/usr/bin/env python3" ]]; then
        echo "Missing python shebang in ${file}"
        RET=1;
    fi
done

exit ${RET}
