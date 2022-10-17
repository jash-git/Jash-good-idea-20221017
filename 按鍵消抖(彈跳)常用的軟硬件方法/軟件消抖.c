四：軟件消抖

我們都知道，並且也是我們使用最多的場合是通過軟件實現按鍵消抖。
最簡單的方式是增加延遲以消除軟件去抖。添加延遲會強制控制器在特定時間段內停止，但在程序中添加延遲並不是一個好的選擇，因為它會暫停程序並增加處理時間。最好的方法是在代碼中使用中斷來進行軟件彈跳。

01
軟件延時



sbit KEY = P1^3;
///按键读取函数
uint8_t GetKey(void)
{
    if(KEY == 1)
    {
        DelayMs(20);        //延时消抖
        if(KEY == 1)
        {
            return 1;
        }
        else 
        {
            return 0;
        }
    }
    else 
    {
        return 0;
    }
}
上面是最簡單的軟件延時方法，也可以通過多個按鍵組合增加相關軟件濾波的方式進行按鍵判斷，其實原理相似。

但是這種純延時的實現方式太過暴力，在延時的時候一直佔用cpu的資源，如果在延時的時候，有其他外部中斷或者搶占事件，系統完全沒有響應的。

所以我們CPU需要一個獨立的定時裝置，來完成這個計時工作，而且需要在計時時間到達時再檢測一次按鍵的電平值。

02
中斷消抖



首先初始化管腳，打開管腳的外部中斷:
/*Configure GPIO pins : KEY_1_Pin KEY_2_Pin */
  GPIO_InitStruct.Pin = KEY_1_Pin|KEY_2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);  
  
  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
 初始化TIM1，打開其update中斷：

static void MX_TIM1_Init(void)
{
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 7200 - 1;                // 72000000 / 7200 = 10000 hz  0.01ms
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 200 - 1;                    // 200 * 0.01 = 20ms
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* htim_base)
{ 
    if(htim_base->Instance==TIM1) 
    {
   /* Peripheral clock enable */                 
        __HAL_RCC_TIM1_CLK_ENABLE();    
  /* USER CODE BEGIN TIM1_MspInit 1 */
        HAL_NVIC_SetPriority(TIM1_UP_IRQn,1,3); 
        HAL_NVIC_EnableIRQ(TIM1_UP_IRQn); 
    }
}
在stm32f1xx_hal_it.c中去註冊中斷回調函數(關鍵的步驟，需要在按鍵中斷處理函數中打開定時器，開始計時)：
void EXTI15_10_IRQHandler(void)            // 按键的中断处理函数
{
 
  HAL_TIM_Base_Start_IT(&htim1);    //  开启定时器1，开始计时
 
  printf("key down\r\n");
 
  __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_11);
  __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_12);
}

定時器的中斷處理函數：
void TIM1_UP_IRQHandler(void)
{
  
  HAL_TIM_IRQHandler(&htim1);   //这个是所有定时器处理回调的入口，在这个函数里对应定时器多种中断情况的中断回调，需要找到update的回调函数
  printf("TIM IRQ\r\n");
 
}
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)        // 定时器update中断处理回调函数
{
   /* USER CODE BEGIN Callback 0 */
 
   /* USER CODE END Callback 0 */
   if (htim->Instance == TIM2) {
     HAL_IncTick();
   }
   
   if (htim->Instance == TIM1) {            // 在这里选择tim1 
 
     printf("TIM1 updata\r\n");
 
    HAL_TIM_Base_Stop_IT(&htim1);       //    关闭tim1 及清除中断
 
     if (GPIO_PIN_RESET == HAL_GPIO_ReadPin(GPIOC,GPIO_PIN_11) )    //再次判断管脚的电平
     {
      printf("KEY1 be pressed!!!\r\n");
     }
    
    if (GPIO_PIN_RESET == HAL_GPIO_ReadPin(GPIOC,GPIO_PIN_12) )//再次判断管脚的电平
    {
      printf("KEY2 be pressed!!!\r\n");
    }
   }
   /* USER CODE BEGIN Callback 1 */
 
   /* USER CODE END Callback 1 */
}


總結一下,實現用定時器中斷來完成按鍵延時去抖的關鍵步驟：

1. 初始化GPIO腳，初始化TIM ，算好時間，填入分頻值。

2. 打開GPIO中斷，在中斷處理函數中打開定時器，讓其計數。

3. 定時器溢出中斷函數中，再次判斷按鍵電平值。關閉定時器，清除pending。

