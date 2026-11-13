[English](README.md) | [日本語](README.ja.md) | [한국어](README.ko.md) | **Bahasa Indonesia** | [Français](README.fr.md) | [繁體中文](README.zh-TW.md)

# VST Ex Audio Filter for OBS

Jendela filter audio tambahan untuk OBS Studio dengan ON/OFF instan untuk siaran bernyanyi, alat latensi, dan
tampilan rantai, ditambah **VST Enigmax Loader**, host **VST2 / VST3** miliknya sendiri. Jendela *Filter (VST Ex)*
bekerja pada rantai filter yang sama dengan jendela *Filter* bawaan OBS, jadi keduanya bisa dipakai.

Dua plugin OBS dipasang: **VST Ex** (jendela, dock, dan filter bawaan) dan **VST Enigmax Loader** (host VST).
Filter VST milik VST Enigmax Loader: jika dihapus, OBS tidak bisa memuatnya lagi. Pindahkan dulu filter VST2 ke filter VST 2.x
bawaan OBS dengan *Serah terima* (lihat di bawah).

- Windows x64, OBS Studio 31 atau lebih baru (diuji di 33.0)
- Pembuat: ChromeBread feat. BlackDream · Lisensi: GPL-2.0

## Instalasi

1. Unduh `OBS-VST-Ex-Audio-Filter-<versi>-windows-x64-setup.exe` dari [Releases](../../releases).
2. Tutup OBS lalu jalankan. *Instalasi penuh* memasang semuanya; *Instalasi kustom* bisa memilih:
   - VST Ex Audio Filter (wajib): *Filter (VST Ex)*, filter pitch / pseudo stereo, Alat > #cbOBScripts > VST Ex Audio Filter
   - VST Enigmax Loader: plugin VST2 / VST3
   - VST Ex Chain View, dock VST Ex Mixer, dock VST Ex Switcher

   Bagian opsional VST Ex bisa dinyalakan / dimatikan nanti di Alat > #cbOBScripts > VST Ex Audio Filter (berlaku setelah OBS dimulai ulang).
3. Jalankan OBS. Saat pertama kali, OBS menanyakan tempat menyimpan thumbnail, tangkapan layar, dan riwayat.

**Portabel (tanpa installer)**: `OBS-VST-Ex-Audio-Filter-<versi>-windows-x64-portable.zip`. Tutup OBS lalu ekstrak
ke `C:\ProgramData\obs-studio\plugins\`: muncul folder `OBS-VST-Ex-Audio-Filter` (VST Ex) dan `vst-enigmax-loader`
(VST Enigmax Loader). Folder yang tidak diperlukan boleh tidak disalin. Untuk menghapus, tutup OBS dan hapus foldernya
(serahterimakan filter VST dulu, lihat di bawah).
Untuk menghapus sekaligus mencadangkan, jalankan `OBS-VST-Ex-Audio-Filter\Safe_Uninstaller\Safe_Uninstaller.bat`.

**Hapus instalasi** (Pengaturan Windows > Aplikasi > VST Ex Audio Filter for OBS). Penghapus instalasi:
1. memperingatkan jika koleksi adegan masih memakai filter VST Enigmax Loader, agar bisa diserahterimakan dulu;
2. menanyakan terpisah apakah menghapus thumbnail plugin, tangkapan layar resolusi tinggi, serta pengaturan /
   preset / riwayat, dengan pilihan mengekspor semuanya dulu;
3. menampilkan file di foldernya yang bukan buatannya (file lain, salinan berganti nama seperti `.bak` / `.org`)
   dan membiarkan Anda menghapus atau menyimpan masing-masing.

## VST Ex Audio Filter

**Jendela Filter (VST Ex)** — Audio Mixer: klik kanan sumber > *Filter (VST Ex)*
- Tombol ON/OFF besar, pilih banyak, seret untuk mengurutkan, duplikat (Ctrl+D), hapus semua, catatan per filter
- Urungkan / Ulangi dan riwayat tersimpan otomatis yang tetap ada setelah OBS dimulai ulang
- Latensi per filter, total latensi, dan tombol untuk mengompensasinya lewat Sync Offset
- **Preset**: simpan / terapkan / ekspor / impor seluruh rantai filter
- **Set ON/OFF** dengan Sync Offset dan volume: ganti verse → chorus seketika di tengah lagu, juga lewat hotkey
- **Ukur dengan 4 ketukan**: lagu tes BPM120 diputar, Anda bertepuk mengikuti (atau tempelkan earphone ke mik) dan Sync Offset dihitung; gelombang yang bisa di-zoom dengan penggaris milidetik untuk penyetelan manual

<p><img src="docs/images/preview-filters.png" alt="Jendela Filter (VST Ex)" width="49%"> <img src="docs/images/preview-filters-select.png" alt="Jendela Filter (VST Ex), beberapa filter dipilih" width="49%"></p>

*Jendela Filter (VST Ex) / Jendela Filter (VST Ex), beberapa filter dipilih*

<p><img src="docs/images/preview-history.png" alt="Riwayat" width="49%"> <img src="docs/images/preview-sync.png" alt="Sync Offset dari 4 ketukan" width="49%"></p>

*Riwayat / Sync Offset dari 4 ketukan*

**Filter bawaan**
- *VST Ex Pitch / Pan*: pitch per semitone dan pan, memudahkan membedakan perangkat saat menyelaraskan
- *VST Ex Pseudo Stereo*: melebarkan mik mono tanpa DAW (efek Haas), satu filter untuk ditambah atau dihapus

- Alat > #cbOBScripts > VST Ex Audio Filter: bahasa (EN / JP / KR / ID / FR / TW), versi, bagian opsional, **Mode debug**

## VST Enigmax Loader

- VST2 dan VST3, pemindaian aman di proses terpisah (dimulai oleh Anda, dengan perkiraan waktu)
- Penjelajah dengan tampilan daftar / gambar mini / detail, urutkan, cari, favorit, tanda HID (sembunyi), catatan, terakhir dipakai
- Gambar mini otomatis saat editor pertama kali dibuka, atau sekaligus; tangkapan layar per filter
- Ekspor / impor pengaturan plugin dari jendela editor
- Thumbnail dan tangkapan layar resolusi tinggi disimpan di folder terpisah
- Salinan plugin di folder berbeda adalah entri terpisah, seperti di OBS: favorit, HID, catatan, dan gambar mini diatur satu per satu

![Pilih Plugin VST (tampilan gambar mini)](docs/images/preview-browser-thumbnails.png)
*Pilih Plugin VST (tampilan gambar mini)*

<p><img src="docs/images/preview-browser-list.png" alt="Pilih Plugin VST (tampilan daftar)" width="49%"> <img src="docs/images/preview-browser-details.png" alt="Pilih Plugin VST (tampilan detail)" width="49%"></p>

*Pilih Plugin VST (tampilan daftar) / Pilih Plugin VST (tampilan detail)*

## VST Ex Chain View

- *VST Ex Chain View* (menu Alat): semua sumber dari perangkat ke output, latensi, level, sidechain, imager setengah lingkaran (opsional), simpan sebagai gambar
- Pilih yang ditampilkan dan disimpan sebagai gambar: hanya sumber aktif, trek 1 sampai 6, serta volume, mono, keseimbangan, Sync Offset, dan pemantauan audio satu per satu
- Seret baris untuk mengurutkan sumber, klik kanan untuk menulis memo di sebelah nama sumber, baris baru setiap N VST

![VST Ex Chain View](docs/images/preview-chain.png)
*VST Ex Chain View*

## VST Ex Mixer

- Dock *VST Ex Mixer*: satu kolom per sumber di bawah meter Audio Mixer, satu tombol per VST

![OBS dengan dock VST Ex Mixer dan VST Ex Switcher](docs/images/preview-obs.png)
*OBS dengan dock VST Ex Mixer dan VST Ex Switcher*

## VST Ex Switcher

- Dock *VST Ex Switcher*: tombol set ON/OFF dan penggunaan CPU (total OBS dan bagian VST)

## VST Ex WaveLine

- Bentuk gelombang sumber pilihan mengalir bertumpuk
- Bilah kiri: zoom (2 sampai 60 detik). Tombol Tr di kanan: sumber pada trek yang menyala dan masuk ke stream tampak pekat, yang lain pudar. Tombol hanya mengubah tampilan, bukan perutean
- `●`: titik ketukan sesuai BPM pilihan (bawaan 120); tekan tepat pada ketukan untuk menyelaraskan. `≡`: urutan lapisan dan opasitas tiap sumber

## Build

Visual Studio 2022 dan CMake 3.28+: `cmake --preset windows-x64` lalu
`cmake --build build_x64 --config RelWithDebInfo`.

VST adalah merek dagang terdaftar milik Steinberg Media Technologies GmbH.

## Tentang pembaruan

Perangkat lunak ini tidak memeriksa pembaruan atau memperbarui dirinya sendiri, dan tidak ada rencana menambahkan fitur pembaruan otomatis yang terhubung ke internet. Untuk memperbarui, ikuti tautan pengembang dan unduh versi baru sendiri dari situs distribusi: [Releases](../../releases)
