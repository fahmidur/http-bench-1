#!/usr/bin/env ruby

require 'fileutils'
require 'json'

duration = 5 # 5 seconds
port = 3535
url_path = "/time"

def print_usage()
  name = File.basename($0)
  puts "Usage: #{name} <server_path>"
end

server_path = ARGV[0]
puts "server_path=#{server_path}"

unless server_path
  print_usage()
  exit 1
end

unless File.exist?(server_path)
  print_usage()
  puts "ERROR: no such file at #{server_path}"
  exit 1
end

def run(cmd)
  puts "RUN: #{cmd}"
  out = `#{cmd}`
  return out
end

def cleanup_key(key)
  key = key.gsub('/', ' per ')
  key = key.gsub(/\s+/, '_')
  key = key.downcase
  return key
end

def cleanup_val(val)
  val = val.strip
  if val =~ /^[\d+\.]+$/
    val = val.to_f
  end
  return val
end

def parse_go_wrk(text)
  data = {}
  lines = text.split("\n")
  lines.each do |line|
    if line =~ /^(.+):\s+(.+)$/
      key = $1
      val = $2
      key = cleanup_key(key)
      data[key] = cleanup_val(val)
    end
  end
  return data
end

data = []
puts "spawning #{server_path}"
pid = Process.spawn("#{server_path} --port #{port}")
sleep 1
1.upto(8).each do |concurrency|
  puts "concurrency=#{concurrency}"
  out = run("go-wrk -c #{concurrency} -d #{duration} http://127.0.0.1:#{port}#{url_path}")
  puts out
  puts "---"
  run_data = parse_go_wrk(out)
  run_data['_duration'] = duration
  run_data['_concurrency'] = concurrency
  puts JSON.pretty_generate(run_data)
  data << run_data
  puts "-"*40
  sleep 1
end
sleep 1
puts "killing pid=#{pid}"
Process.kill(:SIGINT, pid)

report_name = File.basename(File.dirname(server_path))
report_path = "./report/#{report_name}.json"
FileUtils.mkdir_p(File.dirname(report_path))
IO.write(report_path, JSON.pretty_generate(data))

sleep 1
