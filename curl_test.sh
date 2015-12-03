URL=http://api.infinario.com/bulk
FILE1=test_requests/1.json
FILE2=test_requests/2.json
FILE3=test_requests/3.json
FILE4=test_requests/4.json
FILE5=test_requests/5.json
FILE6=test_requests/6.json
FILE7=test_requests/7.json
FILE8=test_requests/8.json
FILE9=test_requests/9.json
FILE10=test_requests/10.json

curl -x 127.0.0.1:8888 -H "Content-Type: application/json" -X POST -d @$FILE1 $URL
curl -x 127.0.0.1:8888 -H "Content-Type: application/json" -X POST -d @$FILE2 $URL
curl -x 127.0.0.1:8888 -H "Content-Type: application/json" -X POST -d @$FILE3 $URL
curl -x 127.0.0.1:8888 -H "Content-Type: application/json" -X POST -d @$FILE4 $URL
curl -x 127.0.0.1:8888 -H "Content-Type: application/json" -X POST -d @$FILE5 $URL
curl -x 127.0.0.1:8888 -H "Content-Type: application/json" -X POST -d @$FILE6 $URL
curl -x 127.0.0.1:8888 -H "Content-Type: application/json" -X POST -d @$FILE7 $URL
curl -x 127.0.0.1:8888 -H "Content-Type: application/json" -X POST -d @$FILE8 $URL
curl -x 127.0.0.1:8888 -H "Content-Type: application/json" -X POST -d @$FILE9 $URL
curl -x 127.0.0.1:8888 -H "Content-Type: application/json" -X POST -d @$FILE10 $URL
