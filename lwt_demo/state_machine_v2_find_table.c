#include <stdio.h>

//当状态、事件非常多，switch case 会很长，就用状态转移表。
//表里面存：[当前状态][事件] = {下一个状态，执行的函数}

typedef enum{
    ST_IDLE, ST_RECV, ST_END, ST_ERR
}State;

typedef enum{
    EVT_S, 
    EVT_NUM, 
    EVT_E, 
    EVT_OTHER
}Event;


// 状态转移表：当前状态 + 事件 → 下一个状态
typedef struct{
    State next_state;
}StateTableItem;

// 二维表：[状态][事件]
StateTableItem table[4][4] = {
    // ST_IDLE
    {{ST_RECV}, {ST_IDLE}, {ST_IDLE}, {ST_IDLE}},
    // ST_RECV
    {{ST_RECV}, {ST_RECV}, {ST_END}, {ST_ERR}},
    // ST_END
    {{ST_IDLE}, {ST_IDLE}, {ST_IDLE}, {ST_IDLE}},
    // ST_ERR
    {{ST_IDLE}, {ST_IDLE}, {ST_IDLE}, {ST_IDLE}}
};

int main()
{
    State cur = ST_IDLE;
    // 根据cur_state和event查表得到next_state
    Event evt = EVT_S;
    State next = table[cur][evt].next_state;
    cur = next;
    printf("next state:%d\n",cur);
    return 0;
}