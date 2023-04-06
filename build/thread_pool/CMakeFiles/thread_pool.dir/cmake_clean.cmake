file(REMOVE_RECURSE
  "libthread_pool.pdb"
  "libthread_pool.a"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/thread_pool.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
