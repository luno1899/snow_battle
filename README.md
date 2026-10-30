# Snowball Round

Нативный плагин для CS2 под **Metamod:Source 2.0.0-git1411 / SourceHook**.

Каждый десятый игровой раунд:

- запрещает команды покупки и ставит `mp_buytime 0`;
- удаляет оружие у живых игроков;
- выдаёт нож и `weapon_decoy` (другой плагин может заменить decoy на снежок);
- включает `sv_infinite_ammo 1` на время раунда;
- показывает по центру HUD сообщение о снежной битве;
- после спецраунда возвращает исходные значения `mp_buytime` и `sv_infinite_ammo`.

Номер раунда берётся из `CCSGameRules::m_totalRoundsPlayed`, поэтому разминка не сдвигает десятый раунд. При недоступности GameRules используется запасной счётчик событий.

## Сборка на GitHub

1. Создайте репозиторий и загрузите в него эти файлы.
2. Откройте вкладку **Actions** и запустите workflow `build` (он также запускается при push).
3. Скачайте артефакт `snowball-round-Linux` или `snowball-round-Windows`.

Workflow сам скачивает Metamod `2.0.0.1411` и совместимый HL2SDK, затем собирает Linux-версию. Локально SDK хранить не нужно.

## Установка

Распакуйте содержимое артефакта в `game/csgo/`. В результате появятся:

```text
game/csgo/addons/metamod/snowball_round.vdf
game/csgo/addons/snowball_round/bin/linuxsteamrt64/snowball_round.so
```

Перезапустите сервер и проверьте `meta list`.

Важно: плагин выдаёт именно `weapon_decoy`. Превращение decoy в снежок остаётся задачей уже установленного у вас второго плагина.
