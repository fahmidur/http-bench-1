#!/usr/bin/env ruby

require 'find'

Find.find('.').each do |path|
  next unless File.basename(path) == 'server.exe'
  puts "path=#{path}"
  system("./bench-one.rb #{path}")
end

