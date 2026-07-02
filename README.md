# SmartLoad ESP32

## Структура проекта

```text
Board/
  SmartLoad/
    SmartLoad.ino      # основной Arduino-скетч ESP32
    config.example.h   # пример локальной конфигурации
    config.h           # локальная конфигурация, не хранится в git
    web/               # исходные HTML-страницы ESP32
    tools/             # вспомогательные скрипты сборки
    libraries/         # локальные библиотеки прошивки
      Sensors/         # измерение тока, напряжения и температуры
      LoadController/  # режимы I = const и P = const, PI-регуляторы и защиты
      HttpInterface/   # веб-интерфейс ESP32 и HTTP-команды
      TelemetryClient/ # отправка телеметрии на сервер отдельной задачей
      SmartLoadWeb/    # сгенерированный заголовок веб-интерфейса ESP32
      OneWire/         # локальная библиотека для DS18B20, не хранится в git

Server/
  server.py            # HTTP-сервер для приема телеметрии и построения графиков
  static/              # HTML, CSS и JS панели телеметрии
```

Перед сборкой после свежего клона создайте локальный конфиг:

```powershell
Copy-Item Board\SmartLoad\config.example.h Board\SmartLoad\config.h
```

После изменения файлов `Board\SmartLoad\web\*.html` пересоберите заголовок для Arduino IDE:

```powershell
python Board\SmartLoad\tools\build_web_header.py
```

## Подключение к ESP32

Wi-Fi точка доступа `ESP32_PANEL` с паролем `12345678`.
Веб-интерфейс ESP32 доступен по адресу:

```text
http://192.168.4.1
```

## Debug-меню ESP32

Настройки PI-регуляторов и шагов PWM доступны на отдельной странице:

```text
http://192.168.4.1/debug
```

Через это меню можно менять коэффициенты `I = const`, `P = const`, максимальный шаг изменения PWM и температуру включения вентилятора.
Настройки применяются во время работы ESP32, но после перезагрузки снова берутся из `config.h`.

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
