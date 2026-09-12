/*==============================================================================
  Лабораторна робота №11. Завдання 2. Варіант 19.
  Тема: обробка бінарних файлів.

  Умова (таблиця 11.2, завдання 19.2): створити масив структур. Кожна
  структура складається з таких елементів: назва фірми, продукт, що
  продається - комп'ютери і програмне забезпечення, регіон збуту, вартість
  продажу, термін постачання. Створений масив структур записати до бінарного
  файла. Виконати такі операції з бінарним файлом:
    - доповнити бінарний файл новими записами;
    - замінити вибраний користувачем запис у бінарному файлі на новий,
      значення полів якого ввести з клавіатури;
    - видалити з бінарного файлу вибраний користувачем запис.
  Здійснити пошук у бінарному файлі та вивести у вигляді таблиць:
    - список комп'ютерів, що продаються у заданому регіоні конкретною фірмою;
    - вартість проданого програмного забезпечення у задані терміни;
    - найрентабельніші фірми (з найбільшою вартістю продажів).
  Результати запитів записати до нового бінарного файлу і вивести на екран.

  Усі дані для запитів беруться з файлу: кожна функція запиту відкриває файл
  даних, послідовно зчитує записи, записує знайдене до окремого файлу
  результатів і закриває обидва файли; на екран виводиться вміст файлу
  результатів.

  Інструментарій: класи потоків fstream, ifstream, ofstream (завдання 1
  реалізовано функціями stdio.h).

  Заміна запису виконується прямим доступом: позиція запису обчислюється як
  номер, помножений на розмір структури, і перезаписується лише цей запис.

  Виконав: Одарчук Олексій, КНУ імені Тараса Шевченка, ФІТ, група ІПЗ-11.

  Компілятор: g++ -std=c++17
==============================================================================*/

#include <iostream>
#include <fstream>
#include <iomanip>
#include <limits>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>

/* Обмеження на розміри даних та імена бінарних файлів. */
const int MAX_RECORDS = 200; /* найбільша кількість записів у файлі даних */
const int MAX_NAME = 32;
const char *DATA_FILE = "sales.dat";
const char *COMPUTERS_FILE = "computers.dat"; /* результат запиту 1 */
const char *SOFTWARE_FILE = "software.dat";   /* результат запиту 2 */
const char *FIRMS_FILE = "firms.dat";         /* результат запиту 3 */

/* Межі року в терміні постачання. */
const int MIN_YEAR = 2000;
const int MAX_YEAR = 2100;

/* Ширини стовпців таблиці записів (у символах). */
const int COL_NUMBER = 4;
const int COL_FIRM = 14;
const int COL_PRODUCT = 16;
const int COL_KIND = 12;
const int COL_REGION = 16;
const int COL_PRICE = 13;
const int COL_DATE = 18;
const int TABLE_WIDTH =
    COL_NUMBER + COL_FIRM + COL_PRODUCT + COL_KIND + COL_REGION + COL_PRICE + COL_DATE;

/* Ширини стовпців таблиці сум продажів по фірмах. */
const int COL_FIRM_NAME = 16;
const int COL_FIRM_SALES = 12;
const int COL_FIRM_TOTAL = 20;
const int FIRM_TABLE_WIDTH = COL_FIRM_NAME + COL_FIRM_SALES + COL_FIRM_TOTAL;

/* Вид продукту: умова прямо називає два види. */
enum class ProductKind
{
    Computer,
    Software
};

/* Термін постачання. */
struct Date
{
    int day;
    int month;
    int year;
};

/*------------------------------------------------------------------------------
  Sale - запис про продаж. Структура має сталий розмір (масиви символів
  замість рядків змінної довжини), що є обов'язковою умовою для запису
  в бінарний файл із прямим доступом: лише за сталого розміру запису
  його позицію можна обчислити множенням номера на sizeof(Sale).
------------------------------------------------------------------------------*/
struct Sale
{
    char firm[MAX_NAME];
    ProductKind kind;
    char product[MAX_NAME];
    char region[MAX_NAME];
    double price;
    Date delivery;
};

/* Сумарні продажі однієї фірми - запис файлу результатів запиту 3. */
struct FirmTotal
{
    char firm[MAX_NAME];
    long sales;
    double total;
};

/*==============================================================================
  Допоміжні функції
==============================================================================*/

/*------------------------------------------------------------------------------
  utf8Width - ширина рядка в символах, а не в байтах (кирилиця в UTF-8
              займає два байти на літеру).
  Параметри: s [вхідний] - рядок.  Повертає: кількість символів.
------------------------------------------------------------------------------*/
int utf8Width(const char *s)
{
    int width = 0;

    for (const unsigned char *p = reinterpret_cast<const unsigned char *>(s);
         *p != '\0'; ++p)
    {
        /* Продовжувальний байт UTF-8 має вигляд 10xxxxxx: маска 0xC0 лишає
           два старші біти, і якщо вони не дорівнюють 10, це початок символу. */
        if ((*p & 0xC0) != 0x80)
        {
            ++width;
        }
    }

    return width;
}

/*------------------------------------------------------------------------------
  printPadded - вивести рядок, доповнивши пропусками до ширини в символах.
  Параметри: s [вхідний], width [вхідний].
------------------------------------------------------------------------------*/
void printPadded(const char *s, int width)
{
    std::cout << s;

    for (int i = utf8Width(s); i < width; ++i)
    {
        std::cout << ' ';
    }
}

/*------------------------------------------------------------------------------
  kindName - назва виду продукту.
  Параметри: kind [вхідний].  Повертає: рядок-константу.
------------------------------------------------------------------------------*/
const char *kindName(ProductKind kind)
{
    return kind == ProductKind::Computer ? "комп'ютери" : "ПЗ";
}

/*------------------------------------------------------------------------------
  dateToNumber - звести дату до числа виду РРРРММДД для порівняння.
  Параметри: d [вхідний] - покажчик на дату.  Повертає: число РРРРММДД.
------------------------------------------------------------------------------*/
long dateToNumber(const Date *d)
{
    return static_cast<long>(d->year) * 10000 + d->month * 100 + d->day;
}

/*------------------------------------------------------------------------------
  formatDate - записати дату до рядка у вигляді ДД.ММ.РРРР.
  Параметри: d [вхідний] - покажчик на дату; buffer [вихідний], size [вхідний].
------------------------------------------------------------------------------*/
void formatDate(const Date *d, char *buffer, size_t size)
{
    std::snprintf(buffer, size, "%02d.%02d.%d", d->day, d->month, d->year);
}

/*------------------------------------------------------------------------------
  daysInMonth - кількість днів у місяці з урахуванням високосного року.
  Параметри: month, year [вхідні].  Повертає: кількість днів.
------------------------------------------------------------------------------*/
int daysInMonth(int month, int year)
{
    if (month == 2)
    {
        const bool isLeap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
        return isLeap ? 29 : 28;
    }
    if (month == 4 || month == 6 || month == 9 || month == 11)
    {
        return 30;
    }
    return 31;
}

/*------------------------------------------------------------------------------
  readInt - прочитати ціле число із заданого діапазону.
  Параметри: prompt [вхідний], value [вихідний], low, high [вхідні].
  Повертає : true - прочитано; false - вхідні дані вичерпано.
------------------------------------------------------------------------------*/
bool readInt(const char *prompt, int *value, int low, int high)
{
    for (;;)
    {
        std::cout << prompt;
        std::cin >> *value;

        if (std::cin.fail() && std::cin.eof())
        {
            return false;
        }

        const bool isNumber = !std::cin.fail();
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        if (isNumber && *value >= low && *value <= high)
        {
            return true;
        }

        std::cout << "Помилка: потрібне ціле число від " << low << " до " << high
                  << ".\n";
    }
}

/*------------------------------------------------------------------------------
  readDouble - прочитати дійсне число, не менше за задану межу.
  Параметри: prompt [вхідний], value [вихідний], low [вхідний].
  Повертає : true - прочитано; false - вхідні дані вичерпано.
------------------------------------------------------------------------------*/
bool readDouble(const char *prompt, double *value, double low)
{
    for (;;)
    {
        std::cout << prompt;
        std::cin >> *value;

        if (std::cin.fail() && std::cin.eof())
        {
            return false;
        }

        const bool isNumber = !std::cin.fail();
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        if (isNumber && *value >= low)
        {
            return true;
        }

        std::cout << "Помилка: потрібне дійсне число, не менше за " << low << ".\n";
    }
}

/*------------------------------------------------------------------------------
  trimIncompleteUtf8 - відкинути неповний символ UTF-8 у кінці рядка.

  Якщо рядок обрізано за розміром буфера, межа може пройти посередині
  багатобайтового символу (кирилична літера займає два байти). Такий
  залишок не є коректним символом, тому відкидається.

  Параметри: s [вхідний/вихідний] - рядок.

  Локальні змінні:
      length   - довжина рядка в байтах;
      lead     - індекс початкового байта останнього символу;
      expected - кількість байтів, яку задає початковий байт.
------------------------------------------------------------------------------*/
void trimIncompleteUtf8(char *s)
{
    const size_t length = std::strlen(s);

    if (length == 0)
    {
        return;
    }

    /* Продовжувальні байти мають вигляд 10xxxxxx: пропускаємо їх до
       початкового байта останнього символу. */
    size_t lead = length - 1;
    while (lead > 0 && (static_cast<unsigned char>(s[lead]) & 0xC0) == 0x80)
    {
        --lead;
    }

    /* Старші біти початкового байта задають довжину послідовності:
       110xxxxx - 2 байти, 1110xxxx - 3, 11110xxx - 4. */
    const unsigned char first = static_cast<unsigned char>(s[lead]);
    size_t expected = 1;
    if ((first & 0xE0) == 0xC0)
    {
        expected = 2;
    }
    else if ((first & 0xF0) == 0xE0)
    {
        expected = 3;
    }
    else if ((first & 0xF8) == 0xF0)
    {
        expected = 4;
    }

    if (length - lead < expected)
    {
        s[lead] = '\0';
    }
}

/*------------------------------------------------------------------------------
  trimSpaces - прибрати пропуски на початку й у кінці рядка, щоб випадковий
               пропуск не заважав порівнянню назв під час пошуку.
  Параметри: s [вхідний/вихідний] - рядок.
------------------------------------------------------------------------------*/
void trimSpaces(char *s)
{
    size_t length = std::strlen(s);

    while (length > 0 && std::isspace(static_cast<unsigned char>(s[length - 1])))
    {
        --length;
    }
    s[length] = '\0';

    size_t start = 0;
    while (std::isspace(static_cast<unsigned char>(s[start])))
    {
        ++start;
    }

    std::memmove(s, s + start, length - start + 1);
}

/*------------------------------------------------------------------------------
  readLine - прочитати текстовий рядок з клавіатури.
  Пропуски на початку й у кінці рядка відкидаються.
  Параметри: prompt [вхідний], buffer [вихідний], size [вхідний].
  Повертає : true - прочитано; false - вхідні дані вичерпано.
------------------------------------------------------------------------------*/
bool readLine(const char *prompt, char *buffer, int size)
{
    std::cout << prompt;
    std::cin.getline(buffer, size);

    if (std::cin.eof() && std::cin.gcount() == 0)
    {
        return false;
    }

    if (std::cin.fail() && !std::cin.eof())
    {
        /* Рядок довший за буфер: зайві символи відкидаються. */
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        trimIncompleteUtf8(buffer);
        std::cout << "Увага: рядок задовгий, збережено лише його початок: " << buffer
                  << '\n';
    }

    trimSpaces(buffer);
    return true;
}

/*------------------------------------------------------------------------------
  readDate - прочитати дату: рік, місяць і день.
  Найбільший припустимий день залежить від місяця й року, тому
  неіснуючу дату (наприклад, 31.02) ввести неможливо.
  Параметри:
      indent [вхідний]  - відступ перед запрошеннями;
      date   [вихідний] - покажчик на дату.
  Повертає : true - прочитано; false - вхідні дані вичерпано.
------------------------------------------------------------------------------*/
bool readDate(const char *indent, Date *date)
{
    char prompt[64];

    std::snprintf(prompt, sizeof prompt, "%sрік (%d..%d): ", indent, MIN_YEAR,
                  MAX_YEAR);
    if (!readInt(prompt, &date->year, MIN_YEAR, MAX_YEAR))
    {
        return false;
    }

    std::snprintf(prompt, sizeof prompt, "%sмісяць (1..12): ", indent);
    if (!readInt(prompt, &date->month, 1, 12))
    {
        return false;
    }

    const int lastDay = daysInMonth(date->month, date->year);
    std::snprintf(prompt, sizeof prompt, "%sдень (1..%d): ", indent, lastDay);
    return readInt(prompt, &date->day, 1, lastDay);
}

/*==============================================================================
  Робота з бінарним файлом
==============================================================================*/

/*------------------------------------------------------------------------------
  recordCount - кількість записів у бінарному файлі.

  Обчислюється як розмір файлу, поділений на розмір однієї структури.
  Такий спосіб можливий саме тому, що всі записи мають однаковий розмір.

  Параметри: fileName [вхідний] - ім'я файлу.
  Повертає : кількість записів або -1, якщо файл не існує.

  Локальні змінні:
      file - вхідний файловий потік;
      size - розмір файлу в байтах.
------------------------------------------------------------------------------*/
long recordCount(const char *fileName)
{
    std::ifstream file(fileName, std::ios::binary | std::ios::ate);

    if (!file)
    {
        return -1;
    }

    const std::streampos size = file.tellg();
    file.close();

    return static_cast<long>(size / static_cast<std::streamoff>(sizeof(Sale)));
}

/*------------------------------------------------------------------------------
  printTableHeader - вивести заголовок таблиці записів.

  Заголовок відповідає переліку полів з умови варіанта і виводиться
  один раз перед даними.
------------------------------------------------------------------------------*/
void printTableHeader()
{
    std::cout << "  ";
    printPadded("№", COL_NUMBER);
    printPadded("Фірма", COL_FIRM);
    printPadded("Продукт", COL_PRODUCT);
    printPadded("Вид", COL_KIND);
    printPadded("Регіон збуту", COL_REGION);
    printPadded("Вартість", COL_PRICE);
    printPadded("Термін постачання", COL_DATE);
    std::cout << "\n  ";

    for (int i = 0; i < TABLE_WIDTH; ++i)
    {
        std::cout << '-';
    }
    std::cout << '\n';
}

/*------------------------------------------------------------------------------
  printRecord - вивести один запис рядком таблиці.
  Параметри: sale [вхідний] - покажчик на структуру; index [вхідний] - номер рядка.
------------------------------------------------------------------------------*/
void printRecord(const Sale *sale, long index)
{
    char buffer[MAX_NAME];

    std::cout << "  ";
    std::snprintf(buffer, sizeof buffer, "%ld.", index);
    printPadded(buffer, COL_NUMBER);
    printPadded(sale->firm, COL_FIRM);
    printPadded(sale->product, COL_PRODUCT);
    printPadded(kindName(sale->kind), COL_KIND);
    printPadded(sale->region, COL_REGION);

    std::snprintf(buffer, sizeof buffer, "%.2f", sale->price);
    printPadded(buffer, COL_PRICE);

    formatDate(&sale->delivery, buffer, sizeof buffer);
    printPadded(buffer, COL_DATE);
    std::cout << '\n';
}

/*------------------------------------------------------------------------------
  printSalesFile - вивести записи бінарного файлу у вигляді таблиці.

  Записи зчитуються послідовно, доки не буде досягнуто кінця файлу.
  Заголовок таблиці виводиться один раз перед першим записом.

  Параметри:
      fileName [вхідний] - ім'я файлу;
      total    [вихідний] - сумарна вартість виведених записів.
  Повертає : кількість записів або -1, якщо файл не існує.
------------------------------------------------------------------------------*/
long printSalesFile(const char *fileName, double *total)
{
    std::ifstream file(fileName, std::ios::binary);

    if (!file)
    {
        return -1;
    }

    Sale sale;
    long count = 0;
    *total = 0.0;

    while (file.read(reinterpret_cast<char *>(&sale), sizeof(Sale)))
    {
        if (count == 0)
        {
            printTableHeader();
        }

        printRecord(&sale, ++count);
        *total += sale.price;
    }

    file.close();
    return count;
}

/*------------------------------------------------------------------------------
  printFirmHeader - вивести заголовок таблиці сум продажів по фірмах.
------------------------------------------------------------------------------*/
void printFirmHeader()
{
    std::cout << "  ";
    printPadded("Фірма", COL_FIRM_NAME);
    printPadded("Продажів", COL_FIRM_SALES);
    printPadded("Сумарна вартість", COL_FIRM_TOTAL);
    std::cout << "\n  ";

    for (int i = 0; i < FIRM_TABLE_WIDTH; ++i)
    {
        std::cout << '-';
    }
    std::cout << '\n';
}

/*------------------------------------------------------------------------------
  printFirmRow - вивести рядок таблиці сум продажів по фірмах.
  Параметри: firm [вхідний] - покажчик на структуру з сумами фірми.
------------------------------------------------------------------------------*/
void printFirmRow(const FirmTotal *firm)
{
    char buffer[MAX_NAME];

    std::cout << "  ";
    printPadded(firm->firm, COL_FIRM_NAME);

    std::snprintf(buffer, sizeof buffer, "%ld", firm->sales);
    printPadded(buffer, COL_FIRM_SALES);

    std::snprintf(buffer, sizeof buffer, "%.2f", firm->total);
    printPadded(buffer, COL_FIRM_TOTAL);
    std::cout << '\n';
}

/* Набори назв для генерації псевдовипадкових записів. */
const char *FIRMS[] = {"Everest", "Kvazar", "Sokil", "Dnipro-IT", "Karpaty"};
const char *REGIONS[] = {"Київський", "Львівський", "Одеський", "Харківський",
                         "Дніпровський"};
const char *COMPUTERS[] = {"Optima 5", "Nova Pro", "Titan X", "Bureau 300"};
const char *SOFTWARE[] = {"OblikPro", "SklavSoft", "DocFlow", "AntiVirus U"};

const int FIRMS_COUNT = sizeof FIRMS / sizeof FIRMS[0];
const int REGIONS_COUNT = sizeof REGIONS / sizeof REGIONS[0];
const int COMPUTERS_COUNT = sizeof COMPUTERS / sizeof COMPUTERS[0];
const int SOFTWARE_COUNT = sizeof SOFTWARE / sizeof SOFTWARE[0];

/*------------------------------------------------------------------------------
  generateRecord - заповнити запис псевдовипадковими даними.
  Параметри: sale [вихідний] - покажчик на структуру.
------------------------------------------------------------------------------*/
void generateRecord(Sale *sale)
{
    std::strcpy(sale->firm, FIRMS[std::rand() % FIRMS_COUNT]);
    std::strcpy(sale->region, REGIONS[std::rand() % REGIONS_COUNT]);

    sale->kind = (std::rand() % 2 == 0) ? ProductKind::Computer : ProductKind::Software;

    if (sale->kind == ProductKind::Computer)
    {
        std::strcpy(sale->product, COMPUTERS[std::rand() % COMPUTERS_COUNT]);
    }
    else
    {
        std::strcpy(sale->product, SOFTWARE[std::rand() % SOFTWARE_COUNT]);
    }

    /* Вартість від 1000,00 до 9999,99 з копійками. */
    sale->price = 1000.0 + (std::rand() % 9000) + (std::rand() % 100) / 100.0;

    sale->delivery.year = 2025;
    sale->delivery.month = 1 + std::rand() % 12;
    sale->delivery.day = 1 + std::rand() % daysInMonth(sale->delivery.month, 2025);
}

/*------------------------------------------------------------------------------
  inputRecord - заповнити запис даними з клавіатури.
  Параметри: sale [вихідний] - покажчик на структуру.
  Повертає : true - заповнено; false - вхідні дані вичерпано.
------------------------------------------------------------------------------*/
bool inputRecord(Sale *sale)
{
    if (!readLine("  Назва фірми: ", sale->firm, MAX_NAME))
    {
        return false;
    }

    int kind = 0;
    if (!readInt("  Вид продукту (1 - комп'ютери, 2 - ПЗ): ", &kind, 1, 2))
    {
        return false;
    }
    sale->kind = (kind == 1) ? ProductKind::Computer : ProductKind::Software;

    if (!readLine("  Назва продукту: ", sale->product, MAX_NAME))
    {
        return false;
    }
    if (!readLine("  Регіон збуту: ", sale->region, MAX_NAME))
    {
        return false;
    }
    if (!readDouble("  Вартість продажу: ", &sale->price, 0.0))
    {
        return false;
    }

    std::cout << "  Термін постачання:\n";
    return readDate("    ", &sale->delivery);
}

/*------------------------------------------------------------------------------
  cmdCreateFile - команда меню: створити масив структур і записати його
                  до бінарного файлу.

  Файл відкривається у режимі std::ios::trunc, тобто попередній вміст
  знищується - команда створює файл заново.
------------------------------------------------------------------------------*/
void cmdCreateFile()
{
    char prompt[64];
    int n = 0;

    std::snprintf(prompt, sizeof prompt,
                  "Уведіть кількість записів (1..%d): ", MAX_RECORDS);
    if (!readInt(prompt, &n, 1, MAX_RECORDS))
    {
        return;
    }

    std::cout << "Спосіб створення:\n"
                 "  1 - введення з клавіатури\n"
                 "  2 - генерація псевдовипадкових даних\n";

    int choice = 0;
    if (!readInt("Оберіть спосіб (1..2): ", &choice, 1, 2))
    {
        return;
    }

    Sale records[MAX_RECORDS];

    if (choice == 1)
    {
        for (int i = 0; i < n; ++i)
        {
            std::cout << "\n  --- запис " << (i + 1) << " ---\n";
            if (!inputRecord(&records[i]))
            {
                return;
            }
        }
    }
    else
    {
        for (int i = 0; i < n; ++i)
        {
            generateRecord(&records[i]);
        }
    }

    std::ofstream file(DATA_FILE, std::ios::binary | std::ios::trunc);

    if (!file)
    {
        std::cout << "Помилка: не вдалося створити файл " << DATA_FILE << ".\n";
        return;
    }

    file.write(reinterpret_cast<const char *>(records), n * sizeof(Sale));
    file.close();

    std::cout << "\nМасив структур записано до бінарного файлу " << DATA_FILE
              << ". Кількість записів: " << n << ".\n";
}

/*------------------------------------------------------------------------------
  cmdPrintFile - команда меню: вивести вміст бінарного файлу.

  Записи зчитуються послідовно, доки не буде досягнуто кінця файлу.
------------------------------------------------------------------------------*/
void cmdPrintFile()
{
    if (recordCount(DATA_FILE) < 0)
    {
        std::cout << "Файл " << DATA_FILE << " не існує. Спочатку створіть його.\n";
        return;
    }

    std::cout << "\nВміст бінарного файлу " << DATA_FILE << "\n\n";

    double total = 0.0;
    const long count = printSalesFile(DATA_FILE, &total);

    std::cout << "\n  Записів у файлі: " << count << '\n';
}

/*------------------------------------------------------------------------------
  cmdAppend - команда меню: доповнити бінарний файл новими записами.

  Файл відкривається в режимі std::ios::app - записи дописуються в кінець,
  наявний вміст зберігається.
------------------------------------------------------------------------------*/
void cmdAppend()
{
    const long total = recordCount(DATA_FILE);

    if (total < 0)
    {
        std::cout << "Файл " << DATA_FILE << " не існує. Спочатку створіть його.\n";
        return;
    }
    if (total >= MAX_RECORDS)
    {
        std::cout << "У файлі вже найбільша допустима кількість записів ("
                  << MAX_RECORDS << ").\n";
        return;
    }

    /* Кількість записів у файлі не може перевищити MAX_RECORDS: на цю межу
       розраховано масиви у функціях видалення та запиту 3. */
    const int freeSlots = MAX_RECORDS - static_cast<int>(total);
    int n = 0;
    char prompt[80];
    std::snprintf(prompt, sizeof prompt,
                  "Скільки записів дописати (1..%d): ", freeSlots);

    if (!readInt(prompt, &n, 1, freeSlots))
    {
        return;
    }

    std::ofstream file(DATA_FILE, std::ios::binary | std::ios::app);

    if (!file)
    {
        std::cout << "Помилка: не вдалося відкрити файл для дописування.\n";
        return;
    }

    for (int i = 0; i < n; ++i)
    {
        std::cout << "\n  --- новий запис " << (i + 1) << " ---\n";

        Sale sale;
        if (!inputRecord(&sale))
        {
            file.close();
            return;
        }

        file.write(reinterpret_cast<const char *>(&sale), sizeof(Sale));
    }

    file.close();

    std::cout << "\nДописано записів: " << n
              << ". Усього у файлі: " << recordCount(DATA_FILE) << ".\n";
}

/*------------------------------------------------------------------------------
  cmdReplace - команда меню: замінити вибраний користувачем запис.

  ПРЯМИЙ ДОСТУП до компонентів бінарного файлу. Позиція запису обчислюється
  як (номер - 1) * sizeof(Sale); покажчик запису встановлюється методом
  seekp(), після чого перезаписується рівно одна структура. Решта файлу
  не переписується - саме в цьому перевага бінарного файлу зі сталим
  розміром запису перед текстовим.
------------------------------------------------------------------------------*/
void cmdReplace()
{
    const long total = recordCount(DATA_FILE);

    if (total < 0)
    {
        std::cout << "Файл " << DATA_FILE << " не існує. Спочатку створіть його.\n";
        return;
    }
    if (total == 0)
    {
        std::cout << "Файл порожній - замінювати нічого.\n";
        return;
    }

    int number = 0;
    char prompt[80];
    std::snprintf(prompt, sizeof prompt, "Номер запису для заміни (1..%ld): ", total);

    if (!readInt(prompt, &number, 1, static_cast<int>(total)))
    {
        return;
    }

    std::cout << "\n  --- новий вміст запису " << number << " ---\n";

    Sale sale;
    if (!inputRecord(&sale))
    {
        return;
    }

    /* Режим in|out відкриває наявний файл без знищення вмісту. */
    std::fstream file(DATA_FILE, std::ios::binary | std::ios::in | std::ios::out);

    if (!file)
    {
        std::cout << "Помилка: не вдалося відкрити файл для запису.\n";
        return;
    }

    file.seekp((number - 1) * sizeof(Sale), std::ios::beg);
    file.write(reinterpret_cast<const char *>(&sale), sizeof(Sale));
    file.close();

    std::cout << "\nЗапис " << number << " замінено.\n";
}

/*------------------------------------------------------------------------------
  cmdDelete - команда меню: видалити вибраний користувачем запис.

  Бінарний файл не має операції вилучення частини вмісту, тому видалення
  виконується перезаписом: усі записи, крім вибраного, послідовно
  переписуються до файлу заново. Це принципова відмінність від заміни,
  яка обходиться прямим доступом.
------------------------------------------------------------------------------*/
void cmdDelete()
{
    const long total = recordCount(DATA_FILE);

    if (total < 0)
    {
        std::cout << "Файл " << DATA_FILE << " не існує. Спочатку створіть його.\n";
        return;
    }
    if (total == 0)
    {
        std::cout << "Файл порожній - видаляти нічого.\n";
        return;
    }
    if (total > MAX_RECORDS)
    {
        std::cout << "Файл містить більше " << MAX_RECORDS
                  << " записів - видалення неможливе.\n";
        return;
    }

    int number = 0;
    char prompt[80];
    std::snprintf(prompt, sizeof prompt,
                  "Номер запису для видалення (1..%ld): ", total);

    if (!readInt(prompt, &number, 1, static_cast<int>(total)))
    {
        return;
    }

    /* Крок 1: зчитати всі записи, крім вибраного. */
    std::ifstream input(DATA_FILE, std::ios::binary);

    if (!input)
    {
        std::cout << "Помилка: не вдалося відкрити файл для читання.\n";
        return;
    }

    Sale records[MAX_RECORDS];
    long kept = 0;
    long index = 0;
    Sale sale;

    while (input.read(reinterpret_cast<char *>(&sale), sizeof(Sale)))
    {
        ++index;
        if (index == number)
        {
            continue; /* вибраний запис пропускається */
        }
        records[kept++] = sale;
    }
    input.close();

    /* Крок 2: перезаписати файл без вилученого запису. */
    std::ofstream output(DATA_FILE, std::ios::binary | std::ios::trunc);

    if (!output)
    {
        std::cout << "Помилка: не вдалося відкрити файл для запису.\n";
        return;
    }

    output.write(reinterpret_cast<const char *>(records), kept * sizeof(Sale));
    output.close();

    std::cout << "\nЗапис " << number << " видалено. Залишилось записів: " << kept
              << ".\n";
}

/*==============================================================================
  Запити. Усі дані беруться з файлу, результат записується до нового
  бінарного файлу і виводиться на екран з цього файлу.
==============================================================================*/

/*------------------------------------------------------------------------------
  cmdQueryComputers - запит 1: список комп'ютерів, що продаються у заданому
                      регіоні конкретною фірмою.

  Знайдені записи записуються до файлу COMPUTERS_FILE, після чого вміст
  цього файлу виводиться на екран.

  Локальні змінні:
      input, output - файл даних і файл результатів;
      region, firm  - ключі пошуку;
      found, total  - кількість і сумарна вартість знайдених записів.
------------------------------------------------------------------------------*/
void cmdQueryComputers()
{
    char region[MAX_NAME];
    char firm[MAX_NAME];

    if (!readLine("Уведіть регіон збуту: ", region, MAX_NAME))
    {
        return;
    }
    if (!readLine("Уведіть назву фірми:  ", firm, MAX_NAME))
    {
        return;
    }

    std::ifstream input(DATA_FILE, std::ios::binary);

    if (!input)
    {
        std::cout << "Файл " << DATA_FILE << " не існує. Спочатку створіть його.\n";
        return;
    }

    std::ofstream output(COMPUTERS_FILE, std::ios::binary | std::ios::trunc);

    if (!output)
    {
        std::cout << "Помилка: не вдалося створити файл " << COMPUTERS_FILE << ".\n";
        return;
    }

    Sale sale;

    while (input.read(reinterpret_cast<char *>(&sale), sizeof(Sale)))
    {
        if (sale.kind == ProductKind::Computer &&
            std::strcmp(sale.region, region) == 0 && std::strcmp(sale.firm, firm) == 0)
        {
            output.write(reinterpret_cast<const char *>(&sale), sizeof(Sale));
        }
    }

    input.close();
    output.close();

    std::cout << "\nЗапит 1. Комп'ютери, що продаються у регіоні \"" << region
              << "\" фірмою \"" << firm << "\"\n"
              << "Результат записано до бінарного файлу " << COMPUTERS_FILE << "\n\n";

    double total = 0.0;
    const long found = printSalesFile(COMPUTERS_FILE, &total);

    if (found <= 0)
    {
        std::cout << "  За заданими ключами пошуку записів не знайдено.\n";
    }
    else
    {
        std::cout << "\n  Знайдено записів: " << found
                  << ", сумарна вартість: " << std::fixed << std::setprecision(2)
                  << total << '\n';
    }
}

/*------------------------------------------------------------------------------
  cmdQuerySoftware - запит 2: вартість проданого програмного забезпечення
                     у задані терміни.

  Знайдені записи записуються до файлу SOFTWARE_FILE, після чого вміст
  цього файлу виводиться на екран.
------------------------------------------------------------------------------*/
void cmdQuerySoftware()
{
    Date from;
    Date to;

    std::cout << "Початок періоду постачання:\n";
    if (!readDate("  ", &from))
    {
        return;
    }

    std::cout << "Кінець періоду постачання:\n";
    if (!readDate("  ", &to))
    {
        return;
    }

    const long fromNumber = dateToNumber(&from);
    const long toNumber = dateToNumber(&to);

    if (fromNumber > toNumber)
    {
        std::cout << "\nПочаток періоду пізніший за його кінець - "
                     "період порожній.\n";
        return;
    }

    std::ifstream input(DATA_FILE, std::ios::binary);

    if (!input)
    {
        std::cout << "Файл " << DATA_FILE << " не існує. Спочатку створіть його.\n";
        return;
    }

    std::ofstream output(SOFTWARE_FILE, std::ios::binary | std::ios::trunc);

    if (!output)
    {
        std::cout << "Помилка: не вдалося створити файл " << SOFTWARE_FILE << ".\n";
        return;
    }

    Sale sale;

    while (input.read(reinterpret_cast<char *>(&sale), sizeof(Sale)))
    {
        const long deliveryNumber = dateToNumber(&sale.delivery);

        if (sale.kind == ProductKind::Software && deliveryNumber >= fromNumber &&
            deliveryNumber <= toNumber)
        {
            output.write(reinterpret_cast<const char *>(&sale), sizeof(Sale));
        }
    }

    input.close();
    output.close();

    char fromText[16];
    char toText[16];
    formatDate(&from, fromText, sizeof fromText);
    formatDate(&to, toText, sizeof toText);

    std::cout << "\nЗапит 2. Програмне забезпечення з терміном постачання з "
              << fromText << " до " << toText << "\n"
              << "Результат записано до бінарного файлу " << SOFTWARE_FILE << "\n\n";

    double total = 0.0;
    const long found = printSalesFile(SOFTWARE_FILE, &total);

    if (found <= 0)
    {
        std::cout << "  У заданий період програмне забезпечення "
                     "не постачалося.\n";
    }
    else
    {
        std::cout << "\n  Знайдено записів: " << found
                  << "\n  Вартість проданого програмного забезпечення: " << std::fixed
                  << std::setprecision(2) << total << '\n';
    }
}

/*------------------------------------------------------------------------------
  cmdQueryFirms - запит 3: найрентабельніші фірми (з найбільшою вартістю
                  продажів).

  Записи зчитуються з файлу й групуються за назвою фірми. ВСІ фірми
  з максимальною сумою (найбільших значень може бути кілька) записуються
  до файлу FIRMS_FILE, після чого вміст цього файлу виводиться на екран.

  Локальні змінні:
      firms     - сумарні продажі кожної фірми;
      firmCount - кількість різних фірм;
      maxTotal  - найбільша сумарна вартість.
------------------------------------------------------------------------------*/
void cmdQueryFirms()
{
    std::ifstream input(DATA_FILE, std::ios::binary);

    if (!input)
    {
        std::cout << "Файл " << DATA_FILE << " не існує. Спочатку створіть його.\n";
        return;
    }

    FirmTotal firms[MAX_RECORDS];
    int firmCount = 0;
    Sale sale;

    while (input.read(reinterpret_cast<char *>(&sale), sizeof(Sale)))
    {
        int position = -1;

        for (int j = 0; j < firmCount; ++j)
        {
            if (std::strcmp(firms[j].firm, sale.firm) == 0)
            {
                position = j;
                break;
            }
        }

        if (position < 0)
        {
            if (firmCount == MAX_RECORDS)
            {
                break; /* файл містить більше записів, ніж допускає програма */
            }

            position = firmCount++;
            std::strcpy(firms[position].firm, sale.firm);
            firms[position].sales = 0;
            firms[position].total = 0.0;
        }

        firms[position].total += sale.price;
        ++firms[position].sales;
    }

    input.close();

    if (firmCount == 0)
    {
        std::cout << "Файл порожній - даних для запиту немає.\n";
        return;
    }

    double maxTotal = firms[0].total;
    for (int j = 1; j < firmCount; ++j)
    {
        if (firms[j].total > maxTotal)
        {
            maxTotal = firms[j].total;
        }
    }

    std::cout << "\nЗапит 3. Сумарна вартість продажів по фірмах\n\n";
    printFirmHeader();
    for (int j = 0; j < firmCount; ++j)
    {
        printFirmRow(&firms[j]);
    }

    std::ofstream output(FIRMS_FILE, std::ios::binary | std::ios::trunc);

    if (!output)
    {
        std::cout << "Помилка: не вдалося створити файл " << FIRMS_FILE << ".\n";
        return;
    }

    for (int j = 0; j < firmCount; ++j)
    {
        if (firms[j].total == maxTotal)
        {
            output.write(reinterpret_cast<const char *>(&firms[j]), sizeof(FirmTotal));
        }
    }

    output.close();

    /* Виведення результату з файлу, до якого його щойно записано. */
    std::ifstream result(FIRMS_FILE, std::ios::binary);

    if (!result)
    {
        std::cout << "Помилка: не вдалося відкрити файл " << FIRMS_FILE << ".\n";
        return;
    }

    std::cout << "\nНайрентабельніші фірми (з найбільшою вартістю продажів)\n"
              << "Результат записано до бінарного файлу " << FIRMS_FILE << "\n\n";
    printFirmHeader();

    FirmTotal firm;
    while (result.read(reinterpret_cast<char *>(&firm), sizeof(FirmTotal)))
    {
        printFirmRow(&firm);
    }

    result.close();
}

/*------------------------------------------------------------------------------
  Головна функція. Відображає меню та викликає відповідні функції.
  Локальні змінні: choice - номер обраного пункту меню.
------------------------------------------------------------------------------*/
int main()
{
    std::cout << "Лабораторна робота №11, завдання 2 (варіант 19)\n"
                 "Виконав: студент групи ІПЗ-11 Одарчук Олексій\n"
                 "Обробка бінарних файлів\n"
                 "Файл даних: "
              << DATA_FILE << "\n";

    /* Генератор псевдовипадкових чисел ініціалізується один раз на весь
       сеанс роботи програми. */
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    for (;;)
    {
        std::cout << "\n============================================================\n"
                     "Меню команд:\n"
                     "  1 - створити масив структур і записати до бінарного файлу\n"
                     "  2 - вивести вміст бінарного файлу\n"
                     "  3 - доповнити файл новими записами\n"
                     "  4 - замінити вибраний запис (прямий доступ)\n"
                     "  5 - видалити вибраний запис\n"
                     "  6 - запит: комп'ютери у заданому регіоні заданої фірми\n"
                     "  7 - запит: вартість проданого ПЗ у задані терміни\n"
                     "  8 - запит: найрентабельніші фірми\n"
                     "  9 - вихід\n";

        int choice = 0;
        if (!readInt("Оберіть команду (1..9): ", &choice, 1, 9))
        {
            std::cout << "\nВхідні дані вичерпано. Завершення роботи.\n";
            break;
        }

        std::cout << '\n';

        if (choice == 1)
        {
            cmdCreateFile();
        }
        else if (choice == 2)
        {
            cmdPrintFile();
        }
        else if (choice == 3)
        {
            cmdAppend();
        }
        else if (choice == 4)
        {
            cmdReplace();
        }
        else if (choice == 5)
        {
            cmdDelete();
        }
        else if (choice == 6)
        {
            cmdQueryComputers();
        }
        else if (choice == 7)
        {
            cmdQuerySoftware();
        }
        else if (choice == 8)
        {
            cmdQueryFirms();
        }
        else
        {
            std::cout << "Завершення роботи.\n";
            break;
        }
    }

    return 0;
}
