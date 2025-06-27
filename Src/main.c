#include<stdio.h>
#include "main.h"
#include "led.h"

// Task Handler Prototypes
void task1_handler(void); 
void task2_handler(void); 
void task3_handler(void); 
void task4_handler(void); 

void init_systick_timer(uint32_t tick_hz);
__attribute__((naked)) void init_scheduler_stack(uint32_t sched_top_of_stack);
void init_tasks_stack(void);
void enable_processor_faults(void);
__attribute__((naked)) void switch_sp_to_psp(void);
uint32_t get_psp_value(void);

void task_delay(uint32_t tick_count);


uint8_t current_task = 1; // Task 1 is currently running

uint32_t g_tick_count = 0;

// TCB contains private info for each task
typedef struct
{
	uint32_t psp_value;
	uint32_t block_count;
	uint8_t  current_state;
	void (*task_handler)(void);
}TCB_t;

TCB_t user_tasks[MAX_TASKS];


int main(void)
{
	enable_processor_faults();

	init_scheduler_stack(SCHED_STACK_START);

	init_tasks_stack();

	led_init_all();

	init_systick_timer(TICK_HZ);

	switch_sp_to_psp();

	task1_handler();

	for(;;);
}



void idle_task(void)
{
	while(1);
}


void task1_handler(void)
{
	while(1)
	{
		led_on(LED_GREEN);
		task_delay(DELAY_COUNT_1S);
		led_off(LED_GREEN);
		delay(DELAY_COUNT_1S);
	}

}


void task2_handler(void)
{
	while(1)
	{
		led_on(LED_ORANGE);
		task_delay(DELAY_COUNT_500MS);
		led_off(LED_ORANGE);
		task_delay(DELAY_COUNT_500MS);
	}

}


void task3_handler(void)
{
	while(1)
	{
		led_on(LED_BLUE);
		task_delay(DELAY_COUNT_250MS);
		led_off(LED_BLUE);
		task_delay(DELAY_COUNT_250MS);
	}

}


void task4_handler(void)
{
	while(1)
	{
		led_on(LED_RED);
		task_delay(DELAY_COUNT_125MS);
		led_off(LED_RED);
		task_delay(DELAY_COUNT_125MS);
	}
}


void init_systick_timer(uint32_t tick_hz)
{
	uint32_t *pSRVR = (uint32_t*)0xE000E014;
	uint32_t *pSCSR = (uint32_t*)0xE000E010;
    
    // Reload val calculation
    uint32_t count_value = (SYSTICK_TIM_CLK/tick_hz)-1;

    // Clear SVR
    *pSRVR &= ~(0x00FFFFFFFF);

    // Load count_value to SVR
    *pSRVR |= count_value;

    *pSCSR |= ( 1 << 1); // Enable the SysTick exception request
    *pSCSR |= ( 1 << 2);  // Indicate that clk source is processor clk

    *pSCSR |= ( 1 << 0); // Enable SysTick counter
}


__attribute__((naked)) void init_scheduler_stack(uint32_t sched_top_of_stack)
{
     __asm volatile("MSR MSP,%0": :  "r" (sched_top_of_stack)  :   );
     __asm volatile("BX LR");
}


// Init task stacks with dummy contents
void init_tasks_stack(void)
{
	user_tasks[0].current_state = TASK_READY_STATE;
	user_tasks[1].current_state = TASK_READY_STATE;
	user_tasks[2].current_state = TASK_READY_STATE;
	user_tasks[3].current_state = TASK_READY_STATE;
	user_tasks[4].current_state = TASK_READY_STATE;

	user_tasks[0].psp_value = IDLE_STACK_START;
	user_tasks[1].psp_value = T1_STACK_START;
	user_tasks[2].psp_value = T2_STACK_START;
	user_tasks[3].psp_value = T3_STACK_START;
	user_tasks[4].psp_value = T4_STACK_START;

	user_tasks[0].task_handler = idle_task;
	user_tasks[1].task_handler = task1_handler;
	user_tasks[2].task_handler = task2_handler;
	user_tasks[3].task_handler = task3_handler;
	user_tasks[4].task_handler = task4_handler;

	uint32_t *pPSP;

	for(int i = 0 ; i < MAX_TASKS ;i++)
	{
		pPSP = (uint32_t*) user_tasks[i].psp_value;

		pPSP--;
		*pPSP = DUMMY_XPSR;//0x01000000

		pPSP--; // PC
		*pPSP = (uint32_t) user_tasks[i].task_handler;

		pPSP--; // LR
		*pPSP = 0xFFFFFFFD;

		for(int j = 0 ; j < 13 ; j++)
		{
			pPSP--;
		    *pPSP = 0;
		}

		user_tasks[i].psp_value = (uint32_t)pPSP;
	}
}


void enable_processor_faults(void)
{
	uint32_t *pSHCSR = (uint32_t*)0xE000ED24;

	*pSHCSR |= ( 1 << 16); // Mem-manage
	*pSHCSR |= ( 1 << 17); // Bus-Fault
	*pSHCSR |= ( 1 << 18); // Usage-Fault
}


uint32_t get_psp_value(void)
{
	return user_tasks[current_task].psp_value;
}


void save_psp_value(uint32_t current_psp_value)
{
	user_tasks[current_task].psp_value = current_psp_value;
}


void update_next_task(void)
{
	int state = TASK_BLOCKED_STATE;

	for(int i= 0 ; i < (MAX_TASKS) ; i++)
	{
		current_task++;
	    current_task %= MAX_TASKS;
		state = user_tasks[current_task].current_state;
		if( (state == TASK_READY_STATE) && (current_task != 0) )
			break;
	}

	if(state != TASK_READY_STATE)
		current_task = 0;
}


__attribute__((naked)) void switch_sp_to_psp(void)
{
    // Init PSP with the stack start address of Task-1
	__asm volatile ("PUSH {LR}"); 
	__asm volatile ("BL get_psp_value");
	__asm volatile ("MSR PSP,R0");
	__asm volatile ("POP {LR}");  

    // Switch SP to PSP 
	__asm volatile ("MOV R0,#0X02");
	__asm volatile ("MSR CONTROL,R0");
	__asm volatile ("BX LR");
}


void schedule(void)
{
	uint32_t *pICSR = (uint32_t*)0xE000ED04; // PendSV pend
	*pICSR |= ( 1 << 28);
}


void task_delay(uint32_t tick_count)
{
	INTERRUPT_DISABLE();

	if(current_task)
	{
	   user_tasks[current_task].block_count = g_tick_count + tick_count;
	   user_tasks[current_task].current_state = TASK_BLOCKED_STATE;
	   schedule();
	}

	INTERRUPT_ENABLE();
}


__attribute__((naked)) void PendSV_Handler(void)
{
    /* Save current task's context: */
    
    // Get current task's PSP val
	__asm volatile("MRS R0,PSP");
    
    // Use PSP to store R4->R11 register values
	__asm volatile("STMDB R0!,{R4-R11}");
	__asm volatile("PUSH {LR}");

	// Save current PSP val
    __asm volatile("BL save_psp_value");

    
	/* Load context of next task */

	// Get next task
    __asm volatile("BL update_next_task");

	// Get its PSP val
	__asm volatile ("BL get_psp_value");

	// Use PSP val to retrieve R4->R11 register values
	__asm volatile ("LDMIA R0!,{R4-R11}");

	// Update PSP
	__asm volatile("MSR PSP,R0");
	__asm volatile("POP {LR}");
	__asm volatile("BX LR");
}


void update_global_tick_count(void)
{
	g_tick_count++;
}


void unblock_tasks(void)
{
	for(int i = 1 ; i < MAX_TASKS ; i++)
	{
		if(user_tasks[i].current_state != TASK_READY_STATE)
		{
			if(user_tasks[i].block_count == g_tick_count)
			{
				user_tasks[i].current_state = TASK_READY_STATE;
			}
		}
	}
}


void  SysTick_Handler(void)
{
	uint32_t *pICSR = (uint32_t*)0xE000ED04;

    update_global_tick_count();

    unblock_tasks();

    *pICSR |= ( 1 << 28); // PendSV pend
}


void HardFault_Handler(void)
{
	printf("Exception : Hardfault\n");
	while(1);
}


void MemManage_Handler(void)
{
	printf("Exception : MemManage\n");
	while(1);
}


void BusFault_Handler(void)
{
	printf("Exception : BusFault\n");
	while(1);
}

