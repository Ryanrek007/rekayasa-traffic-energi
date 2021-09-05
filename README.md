# REKAYASA TRAFFIC ENERGI
***RINGKASAN***
Merupakan salah satu penelitian penulis untuk tugas akhir dalam mencapai sarjana komputer. Dalam penelitian tersebut, melakukan proses perekayasaan sebuah traffic (paket data) yang dapat dikirimkan dari suatu node sumber (source node) menuju node tujuan (sink node) berdasarkan taraf energi yang telah ditentukan pada setiap node yang digunakan dalam simulasi. protokol komunikasi yang digunakan yaitu DSDV (Destination Sequences Distance Vector). Arsitketur jaringan yang digunakan yaitu MANET (Mobile Network Ad-hoc) dengan SIMULATOR NS-3. Adapun hasil yang ingin dicapai yaitu apakah terdapat dampak yang terjadi ketika merekayasa traffic data dengan taraf energi yang telah ditentukan dan mengetahui seberapa efisiensikah perekayasaan tersebut.

adapun sebagian besar proses modifikasi code dalam penelitian tersebut terdapat dalam 2 file:
1. rekayasa-dsdv-energy-model.cc (file Berisi konfigurasi, setelan node, wireless tyoe, topologi, etc.) 
2. dsdv-routing-protocol.cc (file berisi konfigurasi rekayasa energi pada yang dilakukan di dalam protokol DSDV)

Seluruh file telah tersedia secara default pada saat proses instalasi NS-3, tinggal penulis melakukan perekayasaan yang dilakukan dalam penelitian tersebut

full paper: 
https://j-ptiik.ub.ac.id/index.php/j-ptiik/article/view/8224 
