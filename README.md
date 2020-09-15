## Downloader

Пример загрузчика файлов по протоколу HTTP.

## Сборка

Требуется перейти в каталог проекта и выполнить  
```make all -j4```  
В каталоге build/bin появится исполняемый файл: download-file

## Примеры запуска

Получение общей информации  
```build/bin/download-file -h```

Примеры загрузки файлов  
```build/bin/download-file "http://static.svyaznoy.ru/upload/instruction/85e/1000d.pdf"```  
```build/bin/download-file "http://wikireality.ru/w/index.php?title=Livegroups.ru&action=edit&redlink=1"```  
В текущем рабочем каталоге должны появиться соответствующие файлы.
