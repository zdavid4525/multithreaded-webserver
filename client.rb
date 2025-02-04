require 'socket'
st=Process.clock_gettime(Process::CLOCK_MONOTONIC) # time how long it takes to read file and print contents

s = TCPSocket.new 'localhost', 8080
s.write("/tmp/testfiles/#{ARGV[0]}.c\n")

s.each_line do |line|
    # puts line
end
s.close
et=Process.clock_gettime(Process::CLOCK_MONOTONIC)
puts "elapsed: #{et-st} (#{ARGV[0]})\n"