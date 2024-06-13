
module RakefileUtils
  def self.get_os_name
    if RUBY_PLATFORM =~ /darwin/i
      return 'Mac'
    end
    if RUBY_PLATFORM =~ /cygwin|mswin|mingw|bccwin|wince|emx/i
      return 'Win32'
    end
    if RUBY_PLATFORM =~ /linux/i
      return 'Linux'
    end
    return nil
  end

  def self.run(cmd)
    puts "RUN: #{cmd}"
    out = `#{cmd}`
    if out.strip.size > 0
      puts "---\n#{out}\n---"
    end
    if $?.exitstatus != 0
      exit 1
    end
    return out
  end

  def self.compile_cpp(source)
    c_cpp_properties_path = "./c_cpp_properties.json"
    unless File.exist?(c_cpp_properties_path)
      puts "ERROR: expecting c_cpp_properties.json"
      exit 1
    end

    c_cpp_properties = JSON.parse(IO.read('./c_cpp_properties.json'))
    os_name = self.get_os_name()
    unless os_name
      puts "ERROR: could not determine os name"
      exit 1
    end
    # puts "os_name=#{os_name}"
    os_conf = c_cpp_properties['configurations'].find do |obj|
      obj['name'] == os_name
    end

    # puts "os_conf=#{os_conf}"
    unless os_conf
      puts "ERROR: no conf found for os"
      exit 1
    end

    include_flags = os_conf['includePath'].map{|e| "-I#{e}" }.join(' ')
    # puts "include_flags=#{include_flags}"

    target = File.basename(source, File.extname(source)) + '.exe'
    run "g++ -O3 -std=c++20 -o #{target} #{source} #{include_flags} -static"

  end
end
