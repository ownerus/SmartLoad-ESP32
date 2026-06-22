# SmartLoad ESP32

## Структура проекта

```text
Board/
  SmartLoad/
    SmartLoad.ino      # прошивка ESP32 для Arduino IDE

Server/
  server.py            # HTTP-сервер для приема телеметрии и построения графиков
```

## Подключение к ESP32

Wi-Fi точка доступа `ESP32_PANEL` с паролем `12345678`. 
Веб-интерфейс ESP32 доступен по адресу:

```text
http://192.168.4.1
```

## Сервер телеметрии

Запуск сервера:

```bash
cd Server
python server.py
```

Веб-страница сервера доступна на компьютере по адресу:

```text
http://localhost:8000
```