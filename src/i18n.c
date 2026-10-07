#include "i18n.h"

#include <stdio.h>
#include <string.h>

static const char *const k_codes[LANG_COUNT] = { "en", "ar", "es", "fr", "de", "pt", "ru", "ja", "zh", "it", "he" };

static const char *const k_text[LANG_COUNT][MSG_COUNT] = {
    {   /* en */
        "PadBridge: %s connected (slot %d)",
        "PadBridge: %s connected (slot %d), no virtual pad - see the log",
        "PadBridge: %s disconnected (slot %d)",
        "PadBridge: Bluetooth ready (%d paired)",
        "PadBridge: Bluetooth not responding - open the menu for details",
        "PadBridge: Bluetooth lost - press Retry in the menu",
        "PadBridge: pairing for %d s - put the controller in pairing mode",
        "PadBridge: already running",
        "PadBridge %s ready - menu: %s",
        "PadBridge %s - menu unavailable (port %d busy?)",
        "PadBridge: cannot create virtual controllers - see the menu or the log",
        "PadBridge: stopped for rest mode - start it again after waking",
        "PadBridge: stopped",
        "Controller",
    },
    {   /* ar */
        "PadBridge: تم توصيل %s (الخانة %d)",
        "PadBridge: تم توصيل %s (الخانة %d) دون وحدة تحكم افتراضية - راجع السجل",
        "PadBridge: تم فصل %s (الخانة %d)",
        "PadBridge: البلوتوث جاهز (وحدات تحكم مقترنة: %d)",
        "PadBridge: البلوتوث لا يستجيب - افتح القائمة لمعرفة التفاصيل",
        "PadBridge: انقطع البلوتوث - اضغط «إعادة المحاولة» في القائمة",
        "PadBridge: الاقتران لمدة %d ثانية - ضع وحدة التحكم في وضع الاقتران",
        "PadBridge: يعمل بالفعل",
        "PadBridge %s جاهز - القائمة: %s",
        "PadBridge %s - القائمة غير متاحة (هل المنفذ %d مشغول؟)",
        "PadBridge: تعذّر إنشاء وحدات تحكم افتراضية - راجع القائمة أو السجل",
        "PadBridge: توقّف بسبب وضع الراحة - شغّله مجددًا بعد تنشيط الجهاز",
        "PadBridge: تم الإيقاف",
        "وحدة التحكم",
    },
    {   /* es */
        "PadBridge: %s conectado (ranura %d)",
        "PadBridge: %s conectado (ranura %d), sin mando virtual - consulta el registro",
        "PadBridge: %s desconectado (ranura %d)",
        "PadBridge: Bluetooth listo (%d emparejados)",
        "PadBridge: el Bluetooth no responde - abre el menú para ver los detalles",
        "PadBridge: se perdió el Bluetooth - pulsa Reintentar en el menú",
        "PadBridge: emparejando durante %d s - pon el mando en modo de emparejamiento",
        "PadBridge: ya se está ejecutando",
        "PadBridge %s listo - menú: %s",
        "PadBridge %s - menú no disponible (¿puerto %d ocupado?)",
        "PadBridge: no se pueden crear mandos virtuales - consulta el menú o el registro",
        "PadBridge: detenido por el modo de reposo - vuelve a iniciarlo cuando la consola se active",
        "PadBridge: detenido",
        "Mando",
    },
    {   /* fr */
        "PadBridge : %s connectée (emplacement %d)",
        "PadBridge : %s connectée (emplacement %d), sans manette virtuelle - voir le journal",
        "PadBridge : %s déconnectée (emplacement %d)",
        "PadBridge : Bluetooth prêt (manettes associées : %d)",
        "PadBridge : le Bluetooth ne répond pas - ouvrez le menu pour plus de détails",
        "PadBridge : Bluetooth perdu - appuyez sur Réessayer dans le menu",
        "PadBridge : association pendant %d s - mettez la manette en mode association",
        "PadBridge : déjà en cours d'exécution",
        "PadBridge %s prêt - menu : %s",
        "PadBridge %s - menu indisponible (port %d occupé ?)",
        "PadBridge : impossible de créer les manettes virtuelles - voir le menu ou le journal",
        "PadBridge : arrêté pour le mode repos - relancez-le à la sortie du mode repos",
        "PadBridge : arrêté",
        "Manette",
    },
    {   /* de */
        "PadBridge: %s verbunden (Slot %d)",
        "PadBridge: %s verbunden (Slot %d), kein virtueller Controller - siehe Protokoll",
        "PadBridge: %s getrennt (Slot %d)",
        "PadBridge: Bluetooth bereit (%d gekoppelt)",
        "PadBridge: Bluetooth antwortet nicht - Details im Menü",
        "PadBridge: Bluetooth-Verbindung verloren - im Menü auf „Erneut versuchen“ drücken",
        "PadBridge: Kopplung für %d s - Controller in den Kopplungsmodus versetzen",
        "PadBridge: läuft bereits",
        "PadBridge %s bereit - Menü: %s",
        "PadBridge %s - Menü nicht verfügbar (Port %d belegt?)",
        "PadBridge: virtuelle Controller können nicht erstellt werden - siehe Menü oder Protokoll",
        "PadBridge: wegen Ruhemodus beendet - nach dem Aufwecken erneut starten",
        "PadBridge: beendet",
        "Controller",
    },
    {   /* pt */
        "PadBridge: %s conectado (slot %d)",
        "PadBridge: %s conectado (slot %d), sem controle virtual - veja o log",
        "PadBridge: %s desconectado (slot %d)",
        "PadBridge: Bluetooth pronto (%d pareados)",
        "PadBridge: o Bluetooth não responde - abra o menu para ver os detalhes",
        "PadBridge: Bluetooth perdido - pressione Tentar novamente no menu",
        "PadBridge: pareando por %d s - coloque o controle no modo de pareamento",
        "PadBridge: já está em execução",
        "PadBridge %s pronto - menu: %s",
        "PadBridge %s - menu indisponível (porta %d ocupada?)",
        "PadBridge: não foi possível criar controles virtuais - veja o menu ou o log",
        "PadBridge: parado pelo modo de repouso - inicie novamente depois que o console despertar",
        "PadBridge: parado",
        "Controle",
    },
    {   /* ru */
        "PadBridge: %s подключён (слот %d)",
        "PadBridge: %s подключён (слот %d), без виртуального геймпада - см. журнал",
        "PadBridge: %s отключён (слот %d)",
        "PadBridge: Bluetooth готов (сопряжено: %d)",
        "PadBridge: Bluetooth не отвечает - подробности в меню",
        "PadBridge: связь Bluetooth потеряна - нажмите «Повторить» в меню",
        "PadBridge: сопряжение %d с - переведите геймпад в режим сопряжения",
        "PadBridge: уже запущен",
        "PadBridge %s готов - меню: %s",
        "PadBridge %s - меню недоступно (порт %d занят?)",
        "PadBridge: не удалось создать виртуальные геймпады - см. меню или журнал",
        "PadBridge: остановлен из-за режима покоя - запустите снова после выхода из него",
        "PadBridge: остановлен",
        "Геймпад",
    },
    {   /* ja */
        "PadBridge: %s が接続されました（スロット %d）",
        "PadBridge: %s が接続されました（スロット %d）。仮想コントローラーなし - ログを確認してください",
        "PadBridge: %s の接続が切れました（スロット %d）",
        "PadBridge: Bluetooth 準備完了（ペアリング済み %d 台）",
        "PadBridge: Bluetooth が応答しません - 詳細はメニューで確認してください",
        "PadBridge: Bluetooth が切断されました - メニューの「再試行」を押してください",
        "PadBridge: %d 秒間ペアリング中 - コントローラーをペアリングモードにしてください",
        "PadBridge: すでに実行中です",
        "PadBridge %s 準備完了 - メニュー: %s",
        "PadBridge %s - メニューを開けません（ポート %d が使用中？）",
        "PadBridge: 仮想コントローラーを作成できません - メニューかログを確認してください",
        "PadBridge: レストモードのため停止しました - 復帰後にもう一度起動してください",
        "PadBridge: 停止しました",
        "コントローラー",
    },
    {   /* zh */
        "PadBridge：%s 已连接（槽位 %d）",
        "PadBridge：%s 已连接（槽位 %d），但没有虚拟手柄 - 请查看日志",
        "PadBridge：%s 已断开（槽位 %d）",
        "PadBridge：蓝牙已就绪（已配对 %d 个）",
        "PadBridge：蓝牙无响应 - 请打开菜单查看详情",
        "PadBridge：蓝牙连接丢失 - 请在菜单中按“重试”",
        "PadBridge：正在配对，持续 %d 秒 - 请让手柄进入配对模式",
        "PadBridge：已在运行",
        "PadBridge %s 已就绪 - 菜单：%s",
        "PadBridge %s - 菜单不可用（端口 %d 被占用？）",
        "PadBridge：无法创建虚拟手柄 - 请查看菜单或日志",
        "PadBridge：因进入静止模式已停止 - 唤醒后请重新启动",
        "PadBridge：已停止",
        "手柄",
    },
    {   /* it */
        "PadBridge: %s connesso (slot %d)",
        "PadBridge: %s connesso (slot %d), nessun controller virtuale - vedi il registro",
        "PadBridge: %s disconnesso (slot %d)",
        "PadBridge: Bluetooth pronto (%d associati)",
        "PadBridge: il Bluetooth non risponde - apri il menu per i dettagli",
        "PadBridge: Bluetooth perso - premi Riprova nel menu",
        "PadBridge: associazione per %d s - metti il controller in modalità di associazione",
        "PadBridge: già in esecuzione",
        "PadBridge %s pronto - menu: %s",
        "PadBridge %s - menu non disponibile (porta %d occupata?)",
        "PadBridge: impossibile creare i controller virtuali - vedi il menu o il registro",
        "PadBridge: arrestato per la modalità di riposo - avvialo di nuovo dopo il risveglio",
        "PadBridge: arrestato",
        "Controller",
    },
    {   /* he */
        "PadBridge: %s מחובר (חריץ %d)",
        "PadBridge: %s מחובר (חריץ %d), בלי בקר וירטואלי - ראה את היומן",
        "PadBridge: %s התנתק (חריץ %d)",
        "PadBridge: הבלוטות׳ מוכן (מצומדים: %d)",
        "PadBridge: הבלוטות׳ לא מגיב - פתח את התפריט לפרטים",
        "PadBridge: הבלוטות׳ נקטע - לחץ «נסה שוב» בתפריט",
        "PadBridge: צימוד ל-%d שנ׳ - העבר את הבקר למצב צימוד",
        "PadBridge: כבר רץ",
        "PadBridge %s מוכן - תפריט: %s",
        "PadBridge %s - התפריט לא זמין (פורט %d תפוס?)",
        "PadBridge: לא ניתן ליצור בקרי משחק וירטואליים - ראה את התפריט או את היומן",
        "PadBridge: נעצר בגלל מצב מנוחה - הפעל שוב אחרי ההתעוררות",
        "PadBridge: נעצר",
        "בקר",
    },
};

static int g_lang;

int i18n_index(const char *code)
{
    int i;

    if (!code) return -1;
    for (i = 0; i < LANG_COUNT; i++)
        if (strcmp(code, k_codes[i]) == 0) return i;
    return -1;
}

const char *i18n_code(int lang)
{
    return k_codes[lang >= 0 && lang < LANG_COUNT ? lang : 0];
}

int  i18n_get(void) { return g_lang; }

void i18n_set(int lang)
{
    g_lang = lang >= 0 && lang < LANG_COUNT ? lang : 0;
}

const char *tr_in(int lang, int msg)
{
    if (msg < 0 || msg >= MSG_COUNT) return "";
    if (lang < 0 || lang >= LANG_COUNT) lang = 0;
    return k_text[lang][msg];
}

const char *tr(int msg)
{
    return tr_in(g_lang, msg);
}

int i18n_load(const char *path)
{
    char buf[8] = "";
    FILE *f = fopen(path, "r");
    size_t n;

    if (f) {
        n = fread(buf, 1, sizeof buf - 1, f);
        fclose(f);
        buf[n] = '\0';
        buf[strcspn(buf, " \t\r\n")] = '\0';
    }
    i18n_set(i18n_index(buf));
    return g_lang;
}

int i18n_save(const char *path)
{
    char tmp[300];
    FILE *f;
    int ok;

    snprintf(tmp, sizeof tmp, "%s.new", path);
    if (!(f = fopen(tmp, "w"))) return 0;
    ok = fprintf(f, "%s\n", k_codes[g_lang]) > 0;
    if (fclose(f) != 0) ok = 0;
    if (!ok || rename(tmp, path) != 0) {
        remove(tmp);
        return 0;
    }
    return 1;
}
