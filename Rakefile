test_subdirs = [
    "./cpp_boost_1",
    "./cpp_boost_2",
    "./cpp_boost_3",
    "./cpp_boost_4",
    "./golang_1"
]

task :default => :build

desc "Build each test program"
task :build do
    test_subdirs.each do |subdir|
        sh "cd #{subdir} && rake build"
    end
end

desc "Clean everything"
task :clean do
    test_subdirs.each do |subdir|
        sh "cd #{subdir} && rake clean"
    end
end

desc "Benchmark everything"
task :bench => [:build] do
  sh "./bench-all.rb"
  sh  "./graph-all.rb"
end
