#!/usr/bin/env ruby
# coding: utf-8

require 'fileutils'
require 'tempfile'

original_output = "output/original.txt"
parallel_output   = "output/parallel.txt"

def sort_output_file(input_filename)
  results = []
  line_pattern = /^\s*(\d+)\s+(\d+)\s+->.*$/

  begin
    File.foreach(input_filename) do |line|
      match = line_pattern.match(line)
      if match
        results << [[match[1].to_i, match[2].to_i], line.strip]
      end
    end

    results.sort_by! { |item| item[0] }

    temp_file = Tempfile.new(File.basename(input_filename, ".*") + "_sorted_")
    results.each do |item|
      temp_file.puts(item[1])
    end
    temp_file.close
    puts "Sorted results from '#{input_filename}' saved to '#{temp_file.path}'"
    return temp_file

  rescue Errno::ENOENT
    warn "Error: Input file not found: #{input_filename}"
    exit 1
  rescue => e
    warn "Error processing file #{input_filename}: #{e.message}"
    exit 1
  end
end

puts "Sorting sequential output..."
seq_sorted_tempfile = sort_output_file(original_output)

puts "Sorting parallel output..."
par_sorted_tempfile = sort_output_file(parallel_output)

puts "\nComparing sorted outputs..."

diff_command = "diff '#{seq_sorted_tempfile.path}' '#{par_sorted_tempfile.path}'"
diff_output = `#{diff_command}`
diff_status = $?

if diff_status.success? && diff_output.empty?
  puts "Verification Successful: Outputs are identical."
else
  puts "Verification Failed: Outputs differ!"
  puts "Differences found by diff:"
  puts diff_output
  puts "\nCheck the sorted temporary files:"
  puts "- #{seq_sorted_tempfile.path}"
  puts "- #{par_sorted_tempfile.path}"
end


seq_sorted_tempfile.unlink
par_sorted_tempfile.unlink
puts "\nTemporary files cleaned up."

exit(diff_status.success? ? 0 : 1)