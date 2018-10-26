src/test/test_unite --list_content 2>&1 | \
  grep -v -F '    ' | \
  awk '{ print "src/test/test_unite --run_test=" $0 " > /dev/null 2>&1 && echo - [x] " $0 " || echo - [ ] " $0 }' | \
  parallel -j 0 bash -c 2> /dev/null | \
  sort

# for category in $(src/test/test_unite --list_content 2>&1 | grep -v -F '    ')
# do
#   src/test/test_unite --run_test=${category} > /dev/null 2>&1 \
#     && echo '- [x]' ${category} || echo '- [ ]' ${category}
# done

