#if defined(PLATFORM_ESP8266) || defined(PLATFORM_ESP32) || defined (M0139)

#include "ServoMgr.h"
#include "logging.h"
#include "waveform_8266.h"
#include <math.h>

#if defined(M0139)
#include "variant_M0139.h"
extern bool servoInitialized;
static uint8_t GPIO_PIN_PWM_OUTPUTS[GPIO_PIN_PWM_OUTPUTS_COUNT] = {Ch1, Ch2, Ch3, Ch4};

/* Private variables */
TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim3;

/* Private function prototypes */
static void GPIO_Init(void);
static void TIM1_Init(void);
static void TIM3_Init(void);

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
static void PWM_Error_Handler()
{
  /* User can add his own implementation to report the HAL error return state */
  DBGLN("PWM ERROR!");
  __disable_irq();
//   while (1)
//   {
//     /*Write both GREEN and RED LED HIGH to indicate error*/
//     digitalWrite(GPIO_PIN_LED_GREEN, HIGH);
//     digitalWrite(GPIO_PIN_LED, HIGH);
//   }
}
#endif


ServoMgr::ServoMgr(const uint8_t *const pins, const uint8_t outputCnt, uint32_t defaultInterval)
    : _pins(pins), _outputCnt(outputCnt), _refreshInterval(new uint16_t[outputCnt]), _activePwmChannels(0), _resolution_bits(new uint8_t[outputCnt])
{
    for (uint8_t ch = 0; ch < _outputCnt; ++ch)
    {
        _refreshInterval[ch] = defaultInterval;
    }
}

#if defined(PLATFORM_ESP32)
#include "driver/ledc.h"

#ifdef SOC_LEDC_SUPPORT_XTAL_CLOCK
#define LEDC_DEFAULT_CLK LEDC_USE_XTAL_CLK
#else
#define LEDC_DEFAULT_CLK LEDC_AUTO_CLK
#endif

extern uint8_t channels_resolution[];

/*
 * Modified versions of the ledcSetup/ledcAttachPin from Arduino ESP32 Hal which allows
 * control of the timer to use rather than being directly associated with the channel. This
 * allows multiple channels to use the same timer within a LEDC group (high/low speed) when
 * they are set to the same frequency.
 */
static void ledcSetupEx(uint8_t chan, ledc_timer_t timer, uint32_t freq, uint8_t bit_num)
{
    ledc_mode_t group = (ledc_mode_t)(chan / 8);

    ledc_timer_config_t ledc_timer = {
        .speed_mode = group,
        .duty_resolution = (ledc_timer_bit_t)bit_num,
        .timer_num = timer,
        .freq_hz = freq,
        .clk_cfg = LEDC_DEFAULT_CLK};
    if (ledc_timer_config(&ledc_timer) != ESP_OK)
    {
        log_e("ledc setup failed!");
        return;
    }
    channels_resolution[chan] = bit_num;
}

static void ledcAttachPinEx(uint8_t pin, uint8_t chan, ledc_timer_t timer)
{
    uint8_t group = (chan / 8), channel = (chan % 8);

    ledc_channel_config_t ledc_channel = {
        .gpio_num = pin,
        .speed_mode = (ledc_mode_t)group,
        .channel = (ledc_channel_t)channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = (ledc_timer_t)timer,
        .duty = 0,
        .hpoint = 0};
    ledc_channel_config(&ledc_channel);
}

void ServoMgr::allocateLedcChn(uint8_t ch, uint16_t intervalUs, uint8_t pin)
{
    uint32_t target_freq = 1000000U / intervalUs;
    _resolution_bits[ch] = (uint16_t)(log2f(80000000.0f / target_freq)); // no matter high speed timer or low speed timer, the clk src can be 80Mhz
    if (_resolution_bits[ch] > 16)
    {
        _resolution_bits[ch] = 16;
    }

    uint8_t group = ch / 8;
    for (int i = 0; i <= 3; i++)
    {
        uint8_t timer_idx = group * 4 + i;
        if (_timerConfigs[timer_idx] == 0)
        {
            _timerConfigs[timer_idx] = target_freq;
            ledcSetupEx(ch, (ledc_timer_t)i, target_freq, _resolution_bits[ch]);
        }
        if (_timerConfigs[timer_idx] == target_freq)
        {
            ledcAttachPinEx(pin, ch, (ledc_timer_t)i);
            DBGLN("allocate ledc_ch %d on pin %d using group: %d, ledc_tim: %d, bits: %d", ch, pin, group, i, _resolution_bits[ch]);
            return;
        }
    }
    DBGLN("Could not allocate timer for channel %d", ch);
}
#endif

void ServoMgr::initialize()
{
#if defined(M0139)
    /* Init GPIOs, TIM1, TIM3*/
    
    if (!servoInitialized){
        DBGLN("Initializing PWMs BEGIN");
        GPIO_Init();
        TIM1_Init();
        TIM3_Init();
        DBGLN("Initializing PWMs DONE");
    }

    /* Start PWM on each CH at 0us so LEDs on LED Bar are all off */
    for (uint8_t ch = 0; ch < _outputCnt; ++ch)
    {
        const uint8_t pin = _pins[ch];
        if (pin == PIN_AVAILABLE){
            for (int channel = 0; channel < _outputCnt; ++channel){
                if (_pins[channel] == PIN_AVAILABLE){
                DBGLN("Setting %u LOW", GPIO_PIN_PWM_OUTPUTS[channel]);
                // digitalWrite(pin, LOW);
                digitalWrite(GPIO_PIN_PWM_OUTPUTS[channel], LOW);
                    } 
            }
            continue;
        } else{
            DBGLN("_pin[ch:%u] UNAVAILABLE: %u", ch, pin);
        }

        if (pin != PIN_DISCONNECTED && pin != PIN_AVAILABLE)
        {
            switch(pin){
                case Ch1:
                    DBGLN("Starting PWM on: _pins[%u] %u", ch, pin);
                    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
                    __HAL_TIM_SetCompare(&htim3, TIM_CHANNEL_3, 0);  // ALL LEDs OFF
                    break;
                
                case Ch2:
                    DBGLN("Starting PWM on: _pins[%u] %u", ch, pin);
                    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);
                    __HAL_TIM_SetCompare(&htim3, TIM_CHANNEL_4, 0);  // ALL LEDs OFF
                    break;

                case Ch3:
                    DBGLN("Starting PWM on: _pins[%u] %u", ch, pin);
                    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
                    __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_1, 0);  // ALL LEDs OFF
                    break;
                
                case Ch4:
                    DBGLN("Starting PWM on: _pins[%u] %u", ch, pin);
                    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
                    __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_4, 0);  // ALL LEDs OFF
                    break;

                default:
                    DBGLN("Trying to start Undefined pin: _pins[%u] %u", ch, pin);
                    break;
            }
        } else {
            DBGLN("_pins[%u] %u DISCONNECTED", ch, pin);
        }
    }
#elif defined(PLATFORM_ESP8266) || defined(PLATFORM_ESP32)
    for (uint8_t ch = 0; ch < _outputCnt; ++ch)
    {
        const uint8_t pin = _pins[ch];
        if (pin != PIN_DISCONNECTED)
        {
            pinMode(pin, OUTPUT);
            digitalWrite(pin, LOW);
        }
    }
#endif
}

void ServoMgr::writeMicroseconds(uint8_t ch, uint16_t valueUs)
{
    const uint8_t pin = _pins[ch];
    DBGLN("WRITE MICROSECONDS. CH:%u\tPIN:%u\tACTIVE_CHANNELS:%u", ch, pin, _activePwmChannels);
    if (pin == PIN_DISCONNECTED)
    {
        return;
    }
    _activePwmChannels |= (1 << ch);
#if defined(PLATFORM_ESP32)
    ledcWrite(ch, map(valueUs, 0, _refreshInterval[ch], 0, (1 << _resolution_bits[ch]) - 1));
#elif defined(PLATFORM_ESP8266)
    startWaveform8266(pin, valueUs, _refreshInterval[ch] - valueUs);
#elif defined(M0139)
    uint16_t mappedValue = 0;
    DBGLN("Updating PWM on: _pins[%u] %u", ch, pin);
    switch(pin){
        case Ch1:
            mappedValue = map(valueUs, 0, 2100, 0, htim3.Init.Period);
            __HAL_TIM_SetCompare(&htim3, TIM_CHANNEL_3, mappedValue);
            break;
        
        case Ch2:
            mappedValue = map(valueUs, 0, 2100, 0, htim3.Init.Period);
            __HAL_TIM_SetCompare(&htim3, TIM_CHANNEL_4, mappedValue);
            break;

        case Ch3:
            mappedValue = map(valueUs, 0, 2100, 0, htim1.Init.Period);
            __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_1, mappedValue);
            break;
        
        case Ch4:
            mappedValue = map(valueUs, 0, 2100, 0, htim1.Init.Period);
            __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_4, mappedValue);
            break;

        default:
            break;
    }
#endif
}

void ServoMgr::writeDuty(uint8_t ch, uint16_t duty)
{
    const uint8_t pin = _pins[ch];
    if (pin == PIN_DISCONNECTED)
    {
        return;
    }
    _activePwmChannels |= (1 << ch);
#if defined(PLATFORM_ESP32)
    ledcWrite(ch, map(duty, 0, 1000, 0, (1 << _resolution_bits[ch]) - 1));
#elif defined(PLATFORM_ESP8266)
    uint16_t high = map(duty, 0, 1000, 0, _refreshInterval[ch]);
    startWaveform8266(pin, high, _refreshInterval[ch] - high);
#elif defined(M0139)
    uint16_t mappedValue = 0;
    switch(pin){
        case Ch1:
            mappedValue = map(duty, 0, 100, 0, htim3.Init.Period);
            __HAL_TIM_SetCompare(&htim3, TIM_CHANNEL_3, mappedValue);
            break;
        
        case Ch2:
            mappedValue = map(duty, 0, 100, 0, htim3.Init.Period);
            __HAL_TIM_SetCompare(&htim3, TIM_CHANNEL_4, mappedValue);
            break;

        case Ch3:
            mappedValue = map(duty, 0, 100, 0, htim1.Init.Period);
            __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_1, mappedValue);
            break;

        case Ch4:
            mappedValue = map(duty, 0, 100, 0, htim1.Init.Period);
            __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_4, mappedValue);
            break;

        default:
            break;
    }
#endif
}

void ServoMgr::setRefreshInterval(uint8_t ch, uint16_t intervalUs)
{
    if (intervalUs != 0)
    {
        _refreshInterval[ch] = intervalUs;
#if defined(PLATFORM_ESP32)
        const uint8_t pin = _pins[ch];
        if (pin == PIN_DISCONNECTED)
        {
            return;
        }
        allocateLedcChn(ch, intervalUs, pin);
#endif
    }
}

void ServoMgr::stopPwm(uint8_t ch)
{
    const uint8_t pin = _pins[ch];
    if (pin == PIN_DISCONNECTED)
    {
        return;
    }
    _activePwmChannels &= ~(1 << ch);
#if defined(PLATFORM_ESP32)
    ledcDetachPin(pin);
#elif defined(PLATFORM_ESP8266)
    stopWaveform8266(pin);
#elif defined(M0139)
    switch(pin){
        case Ch1:
            __HAL_TIM_SetCompare(&htim3, TIM_CHANNEL_3, 0);
            HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_3);
            digitalWrite(pin, LOW);
            break;
        
        case Ch2:
            __HAL_TIM_SetCompare(&htim3, TIM_CHANNEL_4, 0);
            HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_4);
            digitalWrite(pin, LOW);
            break;

        case Ch3:
            __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_1, 0);
            HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);     
            break;

        case Ch4:
            __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_4, 0);
            HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_4);    
            break;

        default:
            break;
    }
#endif
    digitalWrite(pin, LOW);
}

void ServoMgr::stopAllPwm()
{
    for (uint8_t ch = 0; ch < _outputCnt; ++ch)
    {
        stopPwm(ch);
    }
    _activePwmChannels = 0;
}

void ServoMgr::writeDigital(uint8_t ch, bool value)
{
    const uint8_t pin = _pins[ch];
    if (pin == PIN_DISCONNECTED)
    {
        return;
    }
    if (isPwmActive(ch))
    {
        stopPwm(ch);
        // Wait for the last edge, which is at most 1 cycle from now
        delay((_refreshInterval[ch] / 1000U) + 1);
    }
    digitalWrite(pin, value);
}

#if defined(M0139)

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void GPIO_Init(void)
{
  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
}

void HAL_TIM_MspPostInit(TIM_HandleTypeDef* htim)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(htim->Instance==TIM1)
  {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**TIM1 GPIO Configuration
    PA8     ------> TIM1_CH1
    PA11    ------> TIM1_CH4
    */
    GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  }
  else if(htim->Instance==TIM3)
  {
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**TIM2 GPIO Configuration
    PB0     ------> TIM3_CH3
    PB1     ------> TIM3_CH4
    */
    GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  }
}

/**
  * @brief TIM1 Initialization Function.
  *         ARR = 2068 (Max 2100 us period, 32 clock cycle delay so we use 2068)
  *         900  us HIGH -> 886  us (ARR * DUTY/100)... DUTY is ~42.84% (900/2100)
  *         1400 us HIGH -> 1379 us (ARR * DUTY/100)... DUTY is ~66.66% (1400/2100)
  *         2100 us HIGH -> 2068 us (ARR * DUTY/100)... DUTY is 100% (2100/2100)
  *
  * @param None
  * @retval None
  */
static void TIM1_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 72;        // 72MHz APB2 CLK -> 1MHz APB2 CLK
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 2068;         // 2100 us at 1 MHz 
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    PWM_Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    PWM_Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    PWM_Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    PWM_Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    PWM_Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    PWM_Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    PWM_Error_Handler();
  }
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief TIM2 Initialization Function. 
  *         ARR = 2068 (Max 2100 us period, some delay so we use 2068)
  *         900  us HIGH -> 886  us (ARR * DUTY/100)... DUTY is ~42.84% (900/2100)
  *         1400 us HIGH -> 1379 us (ARR * DUTY/100)... DUTY is ~66.66% (1400/2100)
  *         2100 us HIGH -> 2068 us (ARR * DUTY/100)... DUTY is 100% (2100/2100)
  * 
  * @param None
  * @retval None
  */
static void TIM3_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 72;        
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 2068;         // 2100 us at 1 MHz 
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    PWM_Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    PWM_Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    PWM_Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    PWM_Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    PWM_Error_Handler();
  }
if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    PWM_Error_Handler();
  }
  HAL_TIM_MspPostInit(&htim3);

}
#endif //M0139

#endif