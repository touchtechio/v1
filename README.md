;; This buffer is for notes you don't want to save, and for Lisp evaluation.
;; If you want to create a file, visit that file with C-x C-f,
;; then enter the text in that file's own buffer.


README

./server 127.0.0.1 12000 127.0.0.1 12345 127.0.0.1 12346


./fake_client 127.0.0.1 12000 30 50
XOR
./client 127.0.0.1 12000 -d


./udp_logger 12346
./vis_server 12345
