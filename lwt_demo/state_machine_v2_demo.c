#include <stdio.h>

// 状态
typedef enum {
    ST_IDLE,
    ST_ONE_YUAN,
    ST_DISPENSE,
    ST_ERROR,
    ST_MAX_STATE   // 状态总数，用于数组大小
} MyState;

// 事件
typedef enum {
    EVT_COIN,
    EVT_RESET,
    EVT_MAX_EVENT  // 事件总数
} MyEvent;

// 动作函数原型，发生跳转时执行的业务动作
typedef void (*StateAction)(void);

// 状态表每一项：下一个状态 + 触发时执行的动作
typedef struct {
    MyState next_state;
    StateAction action;
} StateTableEntry;

// -------- 各个业务动作函数 --------
static void action_none(void)
{
    // 无动作
}

static void action_need_more_money(void)
{
    printf("投币1元，还需要1元\n");
}

static void action_dispense_drink(void)
{
    printf("正在送出饮料\n");
}

static void action_reset(void)
{
    printf("执行复位，回到空闲\n");
}

static void action_error_reset(void)
{
    printf("错误，复位回到空闲\n");
}

// ========== 核心状态转移表【二维表：[状态][事件]】 ==========
// 行 = 当前状态；列 = 事件(EVT_COIN, EVT_RESET)
const StateTableEntry state_table[ST_MAX_STATE][EVT_MAX_EVENT] =
{
    // ST_IDLE
    [ST_IDLE][EVT_COIN]  = {ST_ONE_YUAN, action_need_more_money},
    [ST_IDLE][EVT_RESET] = {ST_IDLE,     action_reset},

    // ST_ONE_YUAN
    [ST_ONE_YUAN][EVT_COIN]  = {ST_DISPENSE, action_dispense_drink},
    [ST_ONE_YUAN][EVT_RESET] = {ST_IDLE,     action_reset},

    // ST_DISPENSE
    [ST_DISPENSE][EVT_COIN]  = {ST_IDLE, action_none},
    [ST_DISPENSE][EVT_RESET] = {ST_IDLE, action_reset},

    // ST_ERROR
    [ST_ERROR][EVT_COIN]  = {ST_IDLE, action_error_reset},
    [ST_ERROR][EVT_RESET] = {ST_IDLE, action_error_reset}
};

// 状态机核心：查表驱动
static MyState cur_state = ST_IDLE;

void fsm_handle_event(MyEvent evt)
{
    if(cur_state >= ST_MAX_STATE || evt >= EVT_MAX_EVENT)
    {
        cur_state = ST_IDLE;
        return;
    }

    // 查表！！关键一行
    const StateTableEntry *entry = &state_table[cur_state][evt];

    // 执行动作
    if(entry->action != NULL)
    {
        entry->action();
    }

    // 更新到下一个状态
    cur_state = entry->next_state;
}

int main(void)
{
    // 测试事件序列：投币 → 投币 → 投币 → 复位
    MyEvent evts[] = {EVT_COIN, EVT_COIN, EVT_COIN, EVT_RESET};
    int cnt = sizeof(evts)/sizeof(MyEvent);

    for(int i = 0; i < cnt; i++)
    {
        printf("\n--- 收到事件 %d ---\n", evts[i]);
        fsm_handle_event(evts[i]);
    }
    return 0;
}