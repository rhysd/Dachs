require 'terminfo'

def separator
  "\e[1;33m" + '~' * (TermInfo.screen_size[1]-1) + "\e[0m"
end

def which cmd
  dir = ENV['PATH'].split(':').find {|p| File.executable? File.join(p, cmd)}
  File.join(dir, cmd) unless dir.nil?
end

if which('terminal-notifier')
  notification :terminal_notifier
end

def notify msg
  case
  when which('terminal-notifier')
    `terminal-notifier -message '#{msg}'`
  when which('notify-send')
    `notify-send '#{msg}'`
  when which('tmux')
    `tmux display-message '#{msg}'` if `tmux list-clients 1>/dev/null 2>&1` && $?.success?
  end
end

guard :shell do
  watch %r{(^.+\.(?:hpp|cpp))$} do |m|
    unless m[0] =~ /marching_complete_temp\.cpp$/
      start = Time.now
      puts(separator, start.to_s)

      system "make -j4"
      puts "Elapsed time: #{Time.now - start} seconds."

      if $?.success?
        notify "dachs: Build failed" unless $?.success?
        system "make test"
        unless $?.success?
          notify "dachs: Test failed" unless $?.success?
          puts "---------------LOG-----------------"
          system "cat Testing/Temporary/LastTest.log" if File.exists? "Testing/Temporary/LastTest.log"
        end
      end

      $?.success?
    end
  end

  watch /CMakeLists.txt/ do |m|
    system "cmake ."
  end
end
