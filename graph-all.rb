#!/usr/bin/env ruby

require 'json'

def which(prog)
  out = `which #{prog}`.strip
  if $?.exitstatus != 0
    return nil
  end
  return out
end

def system_open(path)
  if which("open")
    system("open #{path}")
  elsif which("xdg-open")
    system("xdg-open #{path}")
  end
end

labels = []
data = {}

Dir.glob('./report/*.json') do |path|
  puts "path=#{path}"
  server_name = File.basename(path, File.extname(path))
  puts "server_name = #{server_name}"

  run_data = JSON.parse(IO.read(path))
  # puts JSON.pretty_generate(run_data)

  data[server_name] ||= []
  run_data.each do |point_data|
    data[server_name] << point_data['requests_per_sec']
    labels << point_data['_concurrency']
  end

  puts "---"
end
labels = labels.uniq.sort
puts "labels = #{labels}"

pp data

def make_chartjs_datasets(data)
  datasets = []
  data.each do |k, v|
    dataobj = {
      label: k,
      data: v
    }
    datasets << dataobj
  end
  return datasets
end

datasets = make_chartjs_datasets(data)
puts "---"
pp datasets

graph_html_path = "./report/graph.html"
graph_html = File.open(graph_html_path, 'w')
graph_html.puts <<~EOS
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width" />
  <title>Graph</title>
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
</head>
<body>

  <div>
    <canvas id="chart"></canvas>
  </div>

  <script>
    const ctx = document.getElementById('chart');

    new Chart(ctx, {
      type: 'line',
      data: {
        labels: #{labels},
        datasets: #{datasets.to_json}
      },
      options: {
        scales: {
          y: {
            beginAtZero: true,
            title: {
              display: true,
              text: 'requests_per_sec'
            }
          },
          x: {
            title: {
              display: true,
              text: '_concurrency'
            }
          }
        }
      }
    });
  </script>
  
</body>
</html>

EOS

graph_html.close
system_open(graph_html_path)

