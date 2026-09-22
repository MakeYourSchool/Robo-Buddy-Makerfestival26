# MF26 Standalone

Der MF26-Roboter ohne Server: Er macht sein eigenes WLAN auf und bringt seine
Steuerseite selbst mit. Es wird kein Router, kein Rechner und kein Internet
gebraucht.

## Einrichten

```sh
cd standalone
pio run -t upload
```

Eine `secrets.h` gibt es hier nicht, denn der Roboter verbindet sich mit keinem
fremden Netz.

## Verbinden

Das Display führt in zwei Schritten durch:

1. **WLAN:** Den QR-Code mit der Handykamera scannen, dann tritt das Handy dem
   WLAN `MF26-XXXXX` des Roboters bei. Die fünf Zeichen sind der Code des Roboters,
   so bekommt bei mehreren Robotern in einem Raum jeder sein eigenes Netz.
2. **Steuern:** Sobald ein Gerät verbunden ist, wechselt der QR-Code auf die
   Steuerseite `http://192.168.4.1/`. Trennt sich das Gerät wieder, springt die
   Anzeige zurück auf Schritt 1.

Beim ersten echten Fahrbefehl übernimmt das Gesicht. `POST /api/pair` holt die
Anleitung wieder auf das Display.

WLAN-Präfix und Passwort stehen in `src/config.h`. Das Passwort ist nicht
geheim, der QR-Code zeigt es jedem, der vor dem Roboter steht. Es verhindert
nur, dass fremde Handys versehentlich beitreten.

## Unterschiede zur Server-Version

- nur Access Point, kein Verbindungsversuch mit einem bestehenden WLAN
- kein mDNS und keine Discovery; die Adresse ist immer `192.168.4.1`
- keine CORS-Header, weil die Seiten vom Roboter selbst kommen
- `voice-control/` gehört nicht dazu

Steuerseite, Gesichtseditor (`/gesicht`), API und Doku (`/docs`) sind gleich.
