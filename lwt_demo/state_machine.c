#include <stdio.h>

/**
需求说明（非常简单，重点练状态流转）
模拟一个简易投币饮料机，只接受1 元硬币，饮料售价 2 元。
状态定义（自己用 enum 定义）
ST_IDLE：空闲状态，等待投币（初始状态）
ST_ONE_YUAN：已经投入 1 元，还需要再投 1 元
ST_DISPENSE：钱够了，出饮料
ST_ERROR：错误状态

事件（输入）
只有 2 种事件：
EVT_COIN：投入 1 元硬币
EVT_RESET：机器复位，回到空闲

状态转移规则（仔细看，照着这个逻辑写）
ST_IDLE（空闲）
收到 EVT_COIN → 进入 ST_ONE_YUAN
收到 EVT_RESET → 保持 ST_IDLE
ST_ONE_YUAN（已有 1 元）
收到 EVT_COIN → 进入 ST_DISPENSE（钱够，出饮料）
收到 EVT_RESET → 回到 ST_IDLE
ST_DISPENSE（出饮料）

执行动作：打印 “正在送出饮料”

不管什么事件，处理完自动回到 ST_IDLE

ST_ERROR（错误）
收到任意事件，复位回到 ST_IDLE
*/

/*
总结
状态机 = 有限状态 + 事件输入 + 状态跳转；
用一个变量记住现在处于什么模式，外部事件驱动流转；
解决复杂嵌套 if‑else，逻辑可控，嵌入式、协议解析必备；
小逻辑用 switch‑case，大量状态用查表。
*/

#include <stdio.h>

// 1. 定义所有状态
typedef enum {
    ST_IDLE,        // 空闲
    ST_ONE_YUAN,    // 已经投入1元
    ST_DISPENSE,    // 出饮料
    ST_ERROR        // 出错
} MyState;

// 事件
typedef enum {
    EVT_COIN,
    EVT_RESET
} MyEvent;

// 参数：事件数组 + 事件个数
void state_machine_process(MyEvent event[], int len)
{
    MyState cur_state = ST_IDLE;  // 当前状态，初始空闲

    for(int i = 0; i < len; i++)
    {
        MyEvent evt = event[i];   // 取出本次事件！重点
        printf("\n==== 处理第%d个事件，事件:%d，当前状态:%d ====\n", i, evt, cur_state);

        switch (cur_state)
        {
            case ST_IDLE:
                if (evt == EVT_COIN) {
                    cur_state = ST_ONE_YUAN;
                    printf("投币1元，还需要1元\n");
                } else if (evt == EVT_RESET) {
                    cur_state = ST_IDLE;
                    printf("复位，保持空闲\n");
                }
                break;

            case ST_ONE_YUAN:
                if (evt == EVT_COIN){
                    cur_state = ST_DISPENSE;
                } else if (evt == EVT_RESET) {
                    cur_state = ST_IDLE;
                    printf("复位，回到空闲\n");
                }
                break;

            case ST_DISPENSE:
                printf("正在送出饮料\n");
                cur_state = ST_IDLE;
                // 出饮料动作做完，立刻回到空闲
                break;

            case ST_ERROR:
                printf("发生错误，复位回到空闲\n");
                cur_state = ST_IDLE;
                break;

            default:
                printf("非法状态，重置到空闲\n");
                cur_state = ST_IDLE;
                break;
        }
    }
}


int main() {
    // 测试序列：投币 → 投币 → 投币 → 复位
    MyEvent event[] = {EVT_COIN,EVT_COIN,EVT_COIN,EVT_RESET};
    int event_cnt = sizeof(event)/sizeof(MyEvent);
    state_machine_process(event, event_cnt);
    return 0;
}

