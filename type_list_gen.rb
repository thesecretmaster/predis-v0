ht = {}

Dir["types/*"].each do |file|
  fn = File.basename(file, ".*")
  ht[fn] ||= 0
  ht[fn] += 1
end

types = ht.select do |k,v|
  v == 2 && k.ascii_only?
end.map do |k, _v|
  k
end

print types.join("\n")
#File.write("tmp/type_list.gperf", types.join("\n"))
#File.open("tmp/type_list.gperf", "a") do |f|
  #f << "\n%%\n"
  #f << File.read("ht_func.c")
  #f << "\nstatic char* in_word_set(register const char *str, register size_t len);\n"
#end
