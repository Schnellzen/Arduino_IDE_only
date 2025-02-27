//example program how to use millis()
unsigned long previousMillis = 0;
const long interval = 2000; // 2 detik
int state = 0; // Buat nge-track teks yang ditampilkan

void setup() {
  Serial.begin(9600);
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis; // Reset waktu
    if (state == 0) {
      Serial.println("Aku suka nasi");
    } else if (state == 1) {
      Serial.println("Aku suka ayam");
    } else if (state == 2) {
      Serial.println("Aku suka mie");
    }

    state = (state + 1) % 3; // Loop dari 0 ke 1 ke 2 lalu balik ke 0
  }
}

