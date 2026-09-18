#include "bsp_i2c.h"
#include "i2c.h"

#ifndef USE_SOFT_I2C
#define USE_SOFT_I2C 0
#endif

#if USE_SOFT_I2C
/* 鏉烆垯娆㈠Ο鈩冨珯 I2C閵嗗倷琚遍弶鈩冣偓鑽ゅ殠閻ㄥ嫬绱╅懘姘嫲閻炲棛鏁辩憴?bsp_i2c.h閵?*/

typedef struct {
    GPIO_TypeDef *port;
    uint16_t scl;
    uint16_t sda;
} soft_bus_t;

static const soft_bus_t soft_buses[2] = {
    {GPIOB, GPIO_PIN_6, GPIO_PIN_7},     /* &hi2c1閿涙瓌LED */
    {GPIOB, GPIO_PIN_10, GPIO_PIN_11},   /* &hi2c2閿涙艾鐡ㄩ崒?*/
};

/* 閸楀﹣閲滈弮鍫曟寭閸涖劍婀￠惃鍕閺冭绱濋崡鏇氱秴閺勵垰鎯婇悳顖涱偧閺佹壋鈧柡鈧?*娑撳秵妲?CPU 閸涖劍婀?*閿涙碍鐦″▎陇鍑禒锝堢箷閺堝褰夐柌? * 鐠佸潡妫堕妴浣圭槷鏉堝啨鈧線鈧帒顤冮妴浣稿瀻閺€顖ょ礉鐎圭偞绁存稉鈧▎陇鍑禒锝囧 5~16 娑擃亜鎳嗛張鐕傜礉鐟欏棛绱拠鎴犵波閺嬫粏鈧苯鐣鹃妴鍌涘娴? * 閺€閫涚喘閸栨牜鐡戠痪褎鍨ㄩ幑銏㈢椽鐠囨垵娅掗柈鎴掔窗閺€鐟板綁鐎圭偤妾柅鐔哄芳閿涘苯鍨介弬顓熺垼閸戝棛婀呮禒璺ㄦ埂閺冦儱绻旈崪宀勨偓鏄忕帆閸掑棙鐎芥禒顏傗偓? *
 * 娑撹桨绮堟稊鍫滅瑝閻劎鈹栧顏嗗箚閿涙艾甯弶?for (volatile i < N) {} 閻ㄥ嫬鍟撳▔鏇☆潶 Keil 閻?ARMCC 閺佺繝閲? * 娴兼ê瀵查幒澶夌啊閳ユ柡鈧摨roteus 闁插本绁撮崚鎵畱 STOP 瀵よ櫣鐝涢弮鍫曟？閸?30閵?0閵?40 娑撳顫掑顏嗗箚濞嗏剝鏆熸稉瀣厴閺? * 3.110913 us閿涘奔绔村Ο鈥茬閺嶅嚖绱濈拠瀛樻瀵邦亞骞嗛崢瀣壌濞屄ょ獓閵嗗倸濮?__NOP__ 娑斿鎮楃紓鏍槯閸ｃ劍妫ゆ禒搴濈瑓閹靛鈧? *
 * 100 濞嗭紕娈戦柅澶嬪娓氭繃宓侀敍姝卹oteus 閻?I2CMEM 濡€崇€风€?SCL 閻?*濮ｅ繋绔存稉?*妤傛ǜ鈧椒缍嗛悽闈涢挬闁€燁洣
 * 娑撳妾洪敍鍦盌_CLK_HIGH = 4 us閵嗕箑D_CLK_LOW = 4.7 us閿涘绱濇担搴濈艾鐎瑰啯瀵滄担宥嗗珕缂佹繈鈧矮淇婇敍娑溾偓? * OLED 閺勵垰褰熸稉鈧稉顏吥侀崹瀣剁礉鐟曚焦鐪扮€硅姤婢楅悡褎鐗遍懗鎴掑瘨閿涘本澧嶆禒?鐏炲繐绠峰锝呯埗"鐠囦焦妲戞稉宥勭啊"鐎涙ê鍋嶆稊鐔割劀鐢?閵?*/
#define I2C_DELAY_ITERATIONS 100u

static void i2c_delay(void)
{
    for (volatile unsigned i = 0; i < I2C_DELAY_ITERATIONS; ++i) {
        __NOP();
    }
}

/* 閸欍儲鐒洪崣顏嗘暏閺夈儵鈧鈧崵鍤庨敍灞肩瑝妞瑰崬濮╃涵顑挎閵?*/
static const soft_bus_t *bus_of(I2C_HandleTypeDef *handle)
{
    return handle == &hi2c2 ? &soft_buses[1] : &soft_buses[0];
}

static void scl_set(const soft_bus_t *bus, bool high)
{
    HAL_GPIO_WritePin(bus->port, bus->scl, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void sda_set(const soft_bus_t *bus, bool high)
{
    HAL_GPIO_WritePin(bus->port, bus->sda, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* 瀵偓濠曞繐绱╅懘姘辨畱鏉堟挸鍙嗙紓鎾冲暱娑撯偓閻╁瓨婀侀弫鍫礉閹碘偓娴犮儴绶崙鐑樐佸蹇庣瑓鐠囪绶遍崚鎵埂鐎圭偟鍤庨悽闈涢挬閵?*/
static bool sda_get(const soft_bus_t *bus)
{
    return HAL_GPIO_ReadPin(bus->port, bus->sda) == GPIO_PIN_SET;
}

/* 閹?SDA 閻喐顒滈弨鐐灇妤傛﹢妯嗘潏鎾冲弳閵? *
 * 鏉╂瑤绔村銉︽Ц韫囧懘銆忛惃鍕剁礉娑撳秷鍏橀崣顏勫晸 SET 娴滃棔绨ㄩ敍姘磻濠曞繗绶崙鍝勫祮娴ｅ灝鍟?1閿涘苯绱╅懘姘矝鐞氼偅甯归幐鐣岄獓閻ㄥ嫯绶崙? * 缂傛挸鍟块惄鎴ｎ潒閻偓閿涘奔绮犻張鐑樺娴ｅ孩妞?Proteus 娴兼艾鍨界€规碍鍨氭稉銈勯嚋妞瑰崬濮╁┃鎰挨閻㈩煉绱濋幎? * "Logic contention on net"閵嗗倸褰傞柅渚€妯佸▓鍨閺勵垵绶崙鐚寸礉缁涘绨茬粵鏂挎嫲鐠囩粯鏆熼幑顔芥闁€燁洣鐠佲晛绱戦妴?*/
static void sda_input(const soft_bus_t *bus)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = bus->sda;
    gpio.Mode = GPIO_MODE_INPUT;
    /* 韫囧懘銆忕敮锕€鍞撮柈銊ょ瑐閹峰鈧倸鐡ㄩ崒銊︾梾閹恒儲妞傛潻娆愭蒋缁炬寧鐥呴張澶夋崲娴ｆ洖顦婚柈銊ょ瑐閹峰绱濆ù顔锯敄鏉堟挸鍙嗘导姘愁潶鐠囩粯鍨?     * 韫囦粙鐝箛鎴掔秵閻ㄥ嫰娈㈤張鍝勨偓灏栤偓鏂衡偓鏃囶嚢閹存劒缍嗙亸杈潶瑜版挻鍨氭禒搴㈡簚鎼存梻鐡熼敍灞肩艾閺?history_init() 娴犮儰璐?     * 鐎涙ê鍋嶉崷銊у殠閿涘本濡?510 娑擃亝蝎娴ｅ秴鍙忛幍顐＄闁稄绱濋崗澶婂灥婵瀵茬亸杈С閹?7.7 缁夋帇鈧倸鐢稉濠冨娑斿鎮?     * 缁屾椽妫介崡瀹狀嚢娴ｆ粓鐝敍鍦?C 閻ㄥ嫮鈹栭梻鑼暩楠炵绱氶敍宀€婀￠崳銊ゆ閹峰缍嗘惔鏃傜摕閺冩儼鍏橀惄鏍箖鏉╂瑤閲滃鍙樼瑐閹峰鈧?*/
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(bus->port, &gpio);
}

/* 娑撶粯婧€鐟曚線鈹嶉崝?SDA 閺冭泛鍨忛崶鐐茬磻濠曞繗绶崙鎭掆偓?*/
static void sda_output(const soft_bus_t *bus)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = bus->sda;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    /* 鏉堟挸鍤Ο鈥崇础閸掔粯鍓版稉宥呭閻楀洤鍞存稉濠冨閵嗕揪roteus 閻?CM3 濡€崇€烽幎濠傜磻濠曞繐绱╅懘姘瑐閻ㄥ嫪绗傞幏澶婄秼閹存劒绔存稉?     * 妞瑰崬濮╁┃鎰剁礉娴犲孩婧€閹峰缍嗘惔鏃傜摕閺冭泛姘ㄩ幎?"Logic contention on net"閵嗗倽绻栭柌灞肩瑝闂団偓鐟曚礁鐣犻敍姘閺?     * 鐠囪崵鍤庨惃鍕З娴ｆ粣绱檚da_get閿涘鍏橀崣鎴犳晸閸?sda_input() 娑斿鎮楅敍宀冣偓宀勫亝閺壜ょ熅瀵板嫪绗傞幏澶嬫Ц閻ｆ瑧娼冮惃鍕剁礉
     * 鏉堟挸鍤梼鑸殿唽娑撶粯婧€閸欘亜鍟撴稉宥堫嚢閵嗗倸鐤勯悧鈺冩畱濮濓綀顫夐崑姘《娑旂喐妲搁棃鐘衬侀崸妤勫殰鐢妇娈?4.7k 婢舵牠鍎存稉濠冨閵?*/
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(bus->port, &gpio);
}

void bsp_i2c_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    unsigned i;

    /* 先把 CubeMX 初始化好的硬件 I2C 关掉，再接管引脚。只抢 GPIO 不管外设的话，
     * I2C1/I2C2 还使能着，SCL/SDA 上随便一点边沿都会被它当成起始条件去响应，
     * 和下面这套软件时序打架。
     *
     * DeInit 不负责释放引脚——本工程的 hal_msp.c 里没有 I2C 的 MspDeInit，它挂的
     * 是空实现——所以后面那次 HAL_GPIO_Init() 仍然是把引脚从复用功能改成 GPIO
     * 的开漏所必需的一步。 */
    (void)HAL_I2C_DeInit(&hi2c1);
    (void)HAL_I2C_DeInit(&hi2c2);

    __HAL_RCC_GPIOB_CLK_ENABLE();
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_NOPULL;   /* 閻炲棛鏁辩憴?sda_output() */
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Pin = soft_buses[0].scl | soft_buses[0].sda | soft_buses[1].scl | soft_buses[1].sda;
    HAL_GPIO_Init(GPIOB, &gpio);

    for (i = 0; i < 2u; ++i) {
        scl_set(&soft_buses[i], true);
        sda_set(&soft_buses[i], true);
    }
}

static void i2c_start(const soft_bus_t *bus)
{
    sda_output(bus);
    sda_set(bus, true);
    i2c_delay();
    scl_set(bus, true);
    i2c_delay();
    sda_set(bus, false);   /* SCL 妤傛ɑ妞?SDA 娑撳妾?= 鐠у嘲顫愰弶鈥叉 */
    i2c_delay();
    scl_set(bus, false);
    i2c_delay();
}

static void i2c_stop(const soft_bus_t *bus)
{
    sda_output(bus);
    sda_set(bus, false);
    i2c_delay();
    scl_set(bus, true);
    /* 鏉╂瑤绔寸粵澶婃皑閺?tSU;STO閿涙瓔CL 閹额剝鎹ｉ弶銉ょ閸氬氦顩﹂崑婊冾檮閺冨爼妫块幍宥呭帒鐠佺濮?SDA閵?*/
    i2c_delay();
    sda_set(bus, true);
    i2c_delay();
}

/* 閸欐垳绔存稉顏勭摟閼哄偊绱濇潻鏂挎礀娴犲孩婧€閺堝鐥呴張澶婄安缁涙柣鈧?*/
static bool i2c_write_byte(const soft_bus_t *bus, uint8_t byte)
{
    unsigned i;
    bool ack;

    sda_output(bus);
    for (i = 0; i < 8u; ++i) {
        sda_set(bus, (byte & 0x80u) != 0u);
        i2c_delay();
        scl_set(bus, true);
        i2c_delay();
        scl_set(bus, false);
        i2c_delay();
        byte = (uint8_t)(byte << 1);
    }
    /* 缁楊兛绡€娑擃亝妞傞柦鐔活唨缂佹瑤绮犻張鐚寸窗閸忓牊濡?SDA 閺€鐐灇妤傛﹢妯嗛敍灞藉晙閹额剟鐝?SCL 鐠囪绨茬粵鏂烩偓?*/
    sda_input(bus);
    i2c_delay();
    scl_set(bus, true);
    i2c_delay();
    ack = !sda_get(bus);   /* 娴ｅ海鏁搁獮?= 鎼存梻鐡?*/
    scl_set(bus, false);
    i2c_delay();
    sda_output(bus);
    return ack;
}

/* 閺€鏈电娑擃亜鐡ч懞鍌樷偓淇沜k 娑撹櫣婀￠弮鏈靛瘜閺堝搫娲栨惔鏃傜摕閿涘牐绻曢張澶婃倵缂侇厼鐡ч懞鍌︾礆閿涘奔璐熼崑鍥ㄦ閸ョ偤娼惔鏃傜摕閿涘牊娓堕崥搴濈娑擃亷绱氶妴?*/
static uint8_t i2c_read_byte(const soft_bus_t *bus, bool ack)
{
    uint8_t byte = 0u;
    unsigned i;

    sda_input(bus);        /* 閺佸瓨顔岀拠璇插絿閺堢喖妫?SDA 闁棄缍婃禒搴㈡簚妞瑰崬濮?*/
    i2c_delay();
    for (i = 0; i < 8u; ++i) {
        scl_set(bus, true);
        i2c_delay();
        byte = (uint8_t)((byte << 1) | (sda_get(bus) ? 1u : 0u));
        scl_set(bus, false);
        i2c_delay();
    }
    sda_output(bus);       /* 鏉╂瑤绔存担宥囨暠娑撶粯婧€妞瑰崬濮?*/
    sda_set(bus, !ack);
    i2c_delay();
    scl_set(bus, true);
    i2c_delay();
    scl_set(bus, false);
    i2c_delay();
    return byte;
}

/* 閸欐垼顔曟径鍥ф勾閸р偓 + 閸愬懘鍎撮崷鏉挎絻閿?閸?閺傜懓鎮滈妴鍌濈箲閸ョ偘绮犻張鐑樻Ц閸氾箑绨茬粵鏂烩偓?*/
static bool i2c_send_address(const soft_bus_t *bus, uint16_t address, uint16_t offset,
                             uint16_t address_bits)
{
    if (!i2c_write_byte(bus, (uint8_t)address)) {
        return false;
    }
    if (address_bits == 16u) {
        return i2c_write_byte(bus, (uint8_t)(offset >> 8)) &&
               i2c_write_byte(bus, (uint8_t)offset);
    }
    return i2c_write_byte(bus, (uint8_t)offset);
}

bool bsp_i2c_write(I2C_HandleTypeDef *bus_handle, uint16_t address, uint16_t offset,
                   uint16_t address_bits, const uint8_t *data, uint16_t size, uint32_t timeout)
{
    const soft_bus_t *bus = bus_of(bus_handle);
    uint16_t i;
    bool ok;

    (void)timeout;
    i2c_start(bus);
    ok = i2c_send_address(bus, address, offset, address_bits);
    for (i = 0; ok && i < size; ++i) {
        ok = i2c_write_byte(bus, data[i]);
    }
    i2c_stop(bus);
    return ok;
}

bool bsp_i2c_read(I2C_HandleTypeDef *bus_handle, uint16_t address, uint16_t offset,
                  uint16_t address_bits, uint8_t *data, uint16_t size, uint32_t timeout)
{
    const soft_bus_t *bus = bus_of(bus_handle);
    uint16_t i;

    (void)timeout;
    if (size == 0u) {
        return true;
    }
    i2c_start(bus);
    if (!i2c_send_address(bus, address, offset, address_bits)) {
        i2c_stop(bus);
        return false;
    }
    /* 闁插秵鏌婄挧宄邦潗閿涘矁绻栧▎鈩冩Ц鐠囩粯鏌熼崥鎴欌偓?*/
    i2c_start(bus);
    if (!i2c_write_byte(bus, (uint8_t)(address | 1u))) {
        i2c_stop(bus);
        return false;
    }
    for (i = 0; i < size; ++i) {
        /* 閺堚偓閸氬簼绔存稉顏勭摟閼哄倸娲栭棃鐐茬安缁涙棑绱濇稊瀣倵娑撶粯婧€閹靛秷鍏橀崣鎴濅粻濮濄垺娼禒韬测偓?*/
        data[i] = i2c_read_byte(bus, (uint16_t)(i + 1u) < size);
    }
    i2c_stop(bus);
    return true;
}

bool bsp_i2c_ready(I2C_HandleTypeDef *bus_handle, uint16_t address, uint32_t timeout)
{
    const soft_bus_t *bus = bus_of(bus_handle);
    uint32_t waited;

    /* 閹恒垺绁撮崳銊ゆ閸︺劋绗夐崷顭掔礉timeout 閸楁洑缍呴弰顖涱嚑缁夋帇鈧?     *
     * 韫囧懘銆忛惇鐔烘畱闁插秷鐦敍娆礒PROM 閸愭瑥鐣稉鈧稉顏堛€夋稊瀣倵娴兼艾绻?TD_WRITE 閺冨爼妫块敍?4C64 閸忕鐎?5 ms閵?     * Proteus 濡€崇€?6 ms閿涘澧犻柌宥嗘煀鎼存梻鐡熼妴鍌氬晸鐎瑰奔绠ｉ崥搴ｇ彌閸掔粯甯版稉鈧▎掳鈧腐ACK 鐏忓崬缍嬫径杈Е閿涘本妲?     * 閹跺鈧苯娅掓禒鑸殿劀閸︺劌绻栭妴宥堫嚖閸掋倖鍨氶妴灞芥珤娴犳湹绗夐崷銊ｂ偓宥佲偓鏂衡偓鏃囥€冮悳鏉挎皑閺勵垰寮弫鏉跨摠娑撳秳缍囬妴浣稿坊閸欒弓绔撮惄瀛樻Ц
     * 0/510閿涘矁鈧?OLED 閸ョ姳璐熸禒搴濈瑝鐠ф媽绻栭弶陇鐭惧鍕弾鐢憡妯夌粈鐚寸礉鐏炲繐绠峰锝呯埗閸欏秷鈧本甯洪惄鏍︾啊鐎瑰啨鈧?*/
    for (waited = 0u;; ++waited) {
        bool ack;

        i2c_start(bus);
        ack = i2c_write_byte(bus, (uint8_t)address);
        i2c_stop(bus);
        if (ack) {
            return true;
        }
        if (waited >= timeout) {
            return false;
        }
        HAL_Delay(1);
    }
}
#else

void bsp_i2c_init(void)
{
    /* CubeMX has already initialized I2C1 and I2C2. */
}

static uint16_t mem_address_size(uint16_t address_bits)
{
    return address_bits == 16u ? I2C_MEMADD_SIZE_16BIT : I2C_MEMADD_SIZE_8BIT;
}

bool bsp_i2c_write(I2C_HandleTypeDef *bus, uint16_t address, uint16_t offset,
                   uint16_t address_bits, const uint8_t *data, uint16_t size,
                   uint32_t timeout)
{
    return HAL_I2C_Mem_Write(bus, address, offset, mem_address_size(address_bits),
                             (uint8_t *)data, size, timeout) == HAL_OK;
}

bool bsp_i2c_read(I2C_HandleTypeDef *bus, uint16_t address, uint16_t offset,
                  uint16_t address_bits, uint8_t *data, uint16_t size,
                  uint32_t timeout)
{
    return HAL_I2C_Mem_Read(bus, address, offset, mem_address_size(address_bits),
                            data, size, timeout) == HAL_OK;
}

bool bsp_i2c_ready(I2C_HandleTypeDef *bus, uint16_t address, uint32_t timeout)
{
    uint32_t start = HAL_GetTick();
    do {
        if (HAL_I2C_IsDeviceReady(bus, address, 1u, 1u) == HAL_OK) {
            return true;
        }
    } while ((uint32_t)(HAL_GetTick() - start) < timeout);
    return false;
}

#endif
