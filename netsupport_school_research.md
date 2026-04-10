# 📚 Riset: NetSupport School

---

## 1. Apa Itu NetSupport School?

**NetSupport School** adalah software **classroom management** (manajemen kelas) komersial yang dikembangkan oleh **NetSupport Ltd** (Inggris), berdiri sejak **1991**. Software ini dirancang untuk membantu guru/instruktur mengelola, memantau, dan mengontrol komputer siswa di lingkungan lab komputer atau kelas berbasis teknologi.

NetSupport School merupakan salah satu software classroom management **tertua dan paling matang** di pasaran, dengan versi terkini di seri **v15.x**.

> **Intinya**: NetSupport School = Software untuk guru agar bisa melihat layar siswa, mengontrol komputer siswa, memblokir situs/aplikasi, mengirim pesan, dan mengelola kelas secara digital — semuanya lewat jaringan lokal (LAN) atau internet.

---

## 2. Fitur-Fitur Utama

### 🖥️ A. Monitoring & Visibility (Pemantauan)

| Fitur | Deskripsi |
|-------|-----------|
| **Real-time Screen Monitoring** | Melihat thumbnail layar semua siswa secara real-time (termasuk dual monitor) |
| **Application Monitoring** | Melihat aplikasi apa saja yang sedang dijalankan siswa |
| **Website Monitoring** | Melihat website apa yang sedang dibuka siswa |
| **Keyboard Monitoring** | Melihat apa yang diketik siswa |
| **Thumbnail View** | Tampilan grid thumbnail dari semua layar siswa (seperti yang terlihat di screenshot) |

### 📢 B. Instruksi & Kolaborasi

| Fitur | Deskripsi |
|-------|-----------|
| **Screen Broadcast (Show)** | Menampilkan layar guru ke semua layar siswa |
| **Student Screen Show** | Menampilkan layar siswa tertentu ke seluruh kelas |
| **Virtual Whiteboard** | Papan tulis digital terintegrasi |
| **Screen Annotation** | Alat anotasi (garis, panah, shape, highlighter) di atas layar |
| **Audio/Video Support** | Mendukung broadcast audio dan video |
| **Student Journal** | Catatan digital otomatis dari konten pelajaran untuk siswa review |

### 🔒 C. Kontrol Kelas

| Fitur | Deskripsi |
|-------|-----------|
| **Lock Screen** | Mengunci layar siswa agar fokus ke guru |
| **Blank Screen** | Mengosongkan layar siswa |
| **Block All** | Memblokir semua aktivitas siswa |
| **Application Control** | Membatasi/mengizinkan aplikasi tertentu |
| **Internet/Website Control** | Memblokir atau hanya mengizinkan website tertentu |
| **USB/Printer Control** | Mengontrol akses ke perangkat USB dan printer |
| **Power On/Off** | Menyalakan/mematikan komputer siswa secara remote (Wake-on-LAN) |
| **Login/Logout** | Login/logout komputer siswa secara remote |
| **Send Message** | Mengirim pesan teks ke siswa |
| **Send URL** | Membuka URL tertentu di browser siswa |
| **File Distribution/Collection** | Mengirim dan mengumpulkan file dari/ke siswa |
| **Chat** | Fitur chat antara guru dan siswa |
| **Help Requests** | Siswa bisa mengirim permintaan bantuan ke guru |

### 📝 D. Assessment & Gamifikasi

| Fitur | Deskripsi |
|-------|-----------|
| **Quiz/Test Builder** | Membuat kuis dan tes langsung di software |
| **Survey/Polling** | Membuat survei atau polling cepat ke siswa |
| **Instant Feedback** | Feedback instan dari siswa |
| **Gamification** | Fitur gamifikasi untuk meningkatkan partisipasi |
| **Student Register** | Absensi/registrasi siswa digital |

### 🔧 E. Teknis & IT Admin

| Fitur | Deskripsi |
|-------|-----------|
| **Technician Console** | Konsol khusus untuk staf IT mengelola perangkat |
| **Remote Control** | Mengambil alih kendali komputer siswa |
| **User Modes** | Mode Easy, Intermediate, Advanced untuk guru |
| **Lesson Details** | Menyimpan detail pelajaran (guru, nama pelajaran, objektif, outcome) |
| **Saved Classroom Layouts** | Menyimpan layout kelas yang bisa di-load ulang |
| **Silent Deployment** | Instalasi silent via GPO/Intune |

---

## 3. Cara Kerja (Arsitektur)

NetSupport School menggunakan arsitektur **Client-Server** berbasis jaringan TCP/IP:

```
┌─────────────────────────────────────────────────────────────┐
│                    JARINGAN (LAN / Internet)                │
│                                                             │
│  ┌──────────────┐    TCP/UDP     ┌──────────────────────┐   │
│  │  🧑‍🏫 TUTOR     │◄────5405────►│  🧑‍🎓 STUDENT (Agent)  │   │
│  │  (Guru)       │              │  (Siswa - PC 1)       │   │
│  │  Controller   │              └──────────────────────┘   │
│  │               │    TCP/UDP     ┌──────────────────────┐   │
│  │               │◄────5405────►│  🧑‍🎓 STUDENT (Agent)  │   │
│  │               │              │  (Siswa - PC 2)       │   │
│  └──────┬───────┘              └──────────────────────┘   │
│         │                                                   │
│         │ TCP/UDP 5405          ┌──────────────────────┐   │
│         └──────────────────────►│  🧑‍🎓 STUDENT (Agent)  │   │
│                                 │  (Siswa - PC N)       │   │
│                                 └──────────────────────┘   │
│                                                             │
│  ┌──────────────────┐     ┌─────────────────────────┐      │
│  │ 🔧 TECHNICIAN    │     │ 🌐 CONNECTIVITY SERVER  │      │
│  │    Console       │     │    (Gateway/Optional)    │      │
│  │ (Untuk IT Admin) │     │  HTTP/HTTPS port 443     │      │
│  └──────────────────┘     └─────────────────────────┘      │
└─────────────────────────────────────────────────────────────┘
```

### Komponen Utama:

#### 1. **Tutor Console (Guru)**
- Diinstall di komputer guru
- Sebagai **pusat kontrol** untuk memantau dan mengelola semua siswa
- Tampilan Ribbon UI modern (lihat analisis screenshot di bawah)
- Menampilkan thumbnail real-time dari semua layar siswa

#### 2. **Student Agent (Siswa)**
- Diinstall di komputer siswa
- Berjalan di **background** sebagai service/daemon
- Mengirim data layar (screenshot/stream) ke Tutor
- Menerima dan menjalankan perintah dari Tutor (lock, block, dll.)
- Bisa menunjukkan indikator privasi saat sedang dipantau

#### 3. **Technician Console (IT Admin)**
- Konsol khusus untuk staf IT
- Manajemen perangkat, troubleshooting remote
- Kebijakan sekolah secara luas (school-wide policies)

#### 4. **Connectivity Server / Gateway (Opsional)**
- Untuk koneksi antar subnet atau lewat internet
- Menggunakan HTTP/HTTPS (port 443)
- Menghindari konfigurasi firewall yang rumit

### Port Komunikasi:

| Port | Protokol | Fungsi |
|------|----------|--------|
| **5405** | TCP & UDP | Port utama — browsing, koneksi, komunikasi |
| **5421** | TCP/UDP | Multicast/Broadcast Show & distribusi file |
| **25405** | TCP/UDP | Koneksi via Terminal Server |
| **443** | TCP (HTTPS) | Koneksi via Connectivity Server (Gateway) |
| **7 / 9** | UDP | Wake-on-LAN (Power On) |
| **37777** | TCP | Komunikasi dengan Tutor Assistant app |

### Alur Kerja Sederhana:

```
1. Guru membuka Tutor Console
2. Tutor Console scan jaringan / connect ke Room yang sudah di-set
3. Student Agent di komputer siswa merespons (handshake via port 5405)
4. Setelah terhubung:
   - Student Agent secara periodik mengirim screenshot/thumbnail ke Tutor
   - Tutor bisa mengirim command (lock, message, block, dll.)
   - Student Agent menjalankan command tersebut
5. Semua komunikasi via TCP/UDP di jaringan lokal
```

### Mode Koneksi:
- **Room Mode** — Guru connect ke komputer di ruangan tertentu (pre-configured)
- **Browse Mode** — Scan jaringan untuk menemukan Student Agent
- **SIS/User Mode** — Connect berdasarkan user login
- **Gateway Mode** — Lewat Connectivity Server untuk remote/internet

---

## 4. Bahasa Pemrograman

> ⚠️ **NetSupport Ltd tidak mempublikasikan source code atau bahasa pemrograman** yang digunakan karena ini adalah **software proprietary (closed-source)**.

Namun, berdasarkan analisis teknis:

| Aspek | Analisis |
|-------|----------|
| **Core Engine** | Sangat kemungkinan **C atau C++** — karena membutuhkan akses OS-level yang dalam (screen capture, input interception, process management, driver-level hooks) |
| **GUI / Console UI** | Kemungkinan menggunakan **C++ dengan MFC (Microsoft Foundation Classes)** atau **C# dengan WinForms/WPF** untuk tampilan Ribbon UI modern |
| **Networking** | Native Windows Sockets (Winsock) API — TCP/UDP |
| **Cross-platform (ChromeOS)** | Extension berbasis **JavaScript** (Chrome Extension Manifest V3) |
| **Cross-platform (iOS/Android)** | Kemungkinan **Swift/Objective-C** (iOS) dan **Java/Kotlin** (Android) |
| **Web Gateway** | Kemungkinan **C#/.NET** atau **C++** untuk server-side |

### Kenapa C/C++?
1. Software sudah ada **sejak 1991** — era di mana C/C++ dominan untuk Windows development
2. Membutuhkan **performa tinggi** untuk screen capture dan streaming real-time
3. Membutuhkan **akses low-level OS** (hooks, drivers, system services)
4. File executable-nya (.exe) menunjukkan native Windows binary, bukan managed code (.NET)

---

## 5. Analisis Screenshot Tampilan

Berdasarkan screenshot yang diberikan, berikut analisis tampilan **NetSupport School Tutor Console**:

### Layout Utama:

```
┌─────────────────────────────────────────────────────────────┐
│ NetSupport School Tutor                          [_][□][X]  │
├─────────────────────────────────────────────────────────────┤
│ Toolbar: [Refresh] [Connect] [Student] [Send Url]           │
│          [Block All] [Message] [Lock] [Unlock]              │
│          [Chat] [Help Requests]              [About]        │
├──┬──────────────────────────────────────────────────────────┤
│  │ ⊞ All                                                    │
│S │                                                          │
│I │  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐   │
│D │  │ Screen  │  │ Screen  │  │ Screen  │  │ Screen  │   │
│E │  │ Thumb   │  │ Thumb   │  │ Thumb   │  │ Thumb   │   │
│B │  │         │  │         │  │         │  │         │   │
│A │  └─────────┘  └─────────┘  └─────────┘  └─────────┘   │
│R │   Tom Murray   Lisa Scott   Jane Neal   Daniel Woods    │
│  │                                                          │
├──┴──────────────────────────────────────────────────────────┤
│ ⚙ Lesson Details                                            │
│ Teacher: Mr Jones          Lesson Name: English Literature  │
│ Objectives: To understand   Outcome: To be able to assess   │
│ the different styles...     and discuss different language.. │
├─────────────────────────────────────────────────────────────┤
│ Thumbnail Update Speed ─── Thumbnail Size ── Zoom It        │
│ ☑ Show Active Web Site                                      │
│ 🔵 NetSupport Limited ©2019    4 Connected - 9 Selected     │
└─────────────────────────────────────────────────────────────┘
```

### Elemen-elemen yang Terlihat:

| Area | Detail |
|------|--------|
| **Title Bar** | "NetSupport School Tutor" — ini adalah sisi guru |
| **Toolbar** | Icon-icon: Refresh, Connect, Student Register, Send URL, Block All, Message, Lock, Unlock, Chat, Help Requests, About |
| **Sidebar** | Tombol navigasi di sisi kiri (mode tampilan berbeda) |
| **Main Area** | 4 thumbnail layar siswa: Tom Murray, Lisa Scott, Jane Neal, Daniel Woods |
| **Thumbnail** | Menampilkan layar siswa real-time — terlihat Google, YouTube, dll. |
| **Lesson Details** | Panel bawah: Teacher = Mr Jones, Lesson = English Literature, Objectives & Outcome |
| **Status Bar** | "4 Connected - 9 Selected" — 4 siswa terhubung dari 9 yang dipilih |
| **Controls** | Slider Thumbnail Update Speed, Thumbnail Size, checkbox Show Active Web Site |

### Insight dari Screenshot:
1. **4 siswa terhubung** tapi ada **9 yang di-select** → ada 5 siswa yang offline/belum connect
2. **Lisa Scott** terlihat membuka Google → guru bisa memonitor ini
3. **Jane Neal** punya icon merah (⏺) → kemungkinan ada alert/peringatan
4. **Lesson Details** menunjukkan fitur lesson planning terintegrasi
5. Interface terlihat **clean dan minimalis** — desain Windows native

---

## 6. Platform Support

| Platform | Komponen | Status |
|----------|----------|--------|
| **Windows** | Tutor + Student + Technician | ✅ Full Support (Win 7+) |
| **macOS** | Student + Tutor | ✅ Support (64-bit) |
| **ChromeOS** | Student Extension + Tutor App | ✅ Support (ChromeOS 99+) |
| **iOS** | Student App + Tutor Assistant | ✅ Support (iOS 9+) |
| **Android** | Student App + Tutor App | ✅ Support (Android 7.0+) |
| **Linux** | ❌ | ❌ Tidak didukung secara resmi |

---

## 7. Lisensi & Harga

- **Model**: Per workstation/device
- **Harga**: Custom quote (tidak dipublikasikan — harus menghubungi sales)
- **Trial**: Free 30-day evaluation tersedia
- **Deployment**: Mendukung silent install via GPO & Microsoft Intune

---

## 8. Ringkasan Singkat

```
╔══════════════════════════════════════════════════════════════╗
║  NetSupport School — Classroom Management Software          ║
╠══════════════════════════════════════════════════════════════╣
║  Developer    : NetSupport Ltd (UK, sejak 1991)             ║
║  Versi        : v15.x (terkini)                             ║
║  Arsitektur   : Client-Server (Tutor ↔ Student Agent)       ║
║  Protokol     : TCP/UDP port 5405                           ║
║  Bahasa       : Kemungkinan besar C/C++ (core)              ║
║  Platform     : Windows, macOS, ChromeOS, iOS, Android      ║
║  Lisensi      : Proprietary, per-device, custom pricing     ║
║  Fungsi Utama : Monitor layar, kontrol PC, blokir app/web,  ║
║                 broadcast layar, chat, kuis, file transfer   ║
╚══════════════════════════════════════════════════════════════╝
```

---

> 📌 **Catatan**: Informasi ini dikumpulkan dari sumber publik termasuk website resmi NetSupport, review platform, dan analisis teknis. Bahasa pemrograman adalah estimasi berdasarkan karakteristik software karena source code tidak dipublikasikan.
