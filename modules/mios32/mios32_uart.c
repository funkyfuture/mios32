// $Id$
/*
 * U(S)ART functions for MIOS32
 *
 * TODO: provide alternative baudrates for MIDI-to-COM, but also
 * for UART based OSC (we could also provide a mios32_uart_osc layer!)
 *
 * ==========================================================================
 *
 *  Copyright (C) 2008 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

/////////////////////////////////////////////////////////////////////////////
// Include files
/////////////////////////////////////////////////////////////////////////////

#include <mios32.h>

// this module can be optionally disabled in a local mios32_config.h file (included from mios32.h)
#if !defined(MIOS32_DONT_USE_UART)

#include <FreeRTOS.h>
#include <portmacro.h>


/////////////////////////////////////////////////////////////////////////////
// Pin definitions and USART mappings
/////////////////////////////////////////////////////////////////////////////

#define MIOS32_UART0_TX_PORT     GPIOA
#define MIOS32_UART0_TX_PIN      GPIO_Pin_9
#define MIOS32_UART0_RX_PORT     GPIOA
#define MIOS32_UART0_RX_PIN      GPIO_Pin_10
#define MIOS32_UART0             USART1
#define MIOS32_UART0_IRQ_CHANNEL USART1_IRQChannel
#define MIOS32_UART0_IRQHANDLER_FUNC void USART1_IRQHandler(void)
#define MIOS32_UART0_REMAP_FUNC  {}

#define MIOS32_UART1_TX_PORT     GPIOC
#define MIOS32_UART1_TX_PIN      GPIO_Pin_10
#define MIOS32_UART1_RX_PORT     GPIOC
#define MIOS32_UART1_RX_PIN      GPIO_Pin_11
#define MIOS32_UART1             USART3
#define MIOS32_UART1_IRQ_CHANNEL USART3_IRQChannel
#define MIOS32_UART1_IRQHANDLER_FUNC void USART3_IRQHandler(void)
#define MIOS32_UART1_REMAP_FUNC  { GPIO_PinRemapConfig(GPIO_PartialRemap_USART3, ENABLE); }


/////////////////////////////////////////////////////////////////////////////
// Local definitions
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Global variables
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Local Prototypes
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Local types
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

#if MIOS32_UART_NUM >= 1
static u8 rx_buffer[MIOS32_UART_NUM][MIOS32_UART_RX_BUFFER_SIZE];
static volatile u8 rx_buffer_tail[MIOS32_UART_NUM];
static volatile u8 rx_buffer_head[MIOS32_UART_NUM];
static volatile u8 rx_buffer_size[MIOS32_UART_NUM];

static u8 tx_buffer[MIOS32_UART_NUM][MIOS32_UART_TX_BUFFER_SIZE];
static volatile u8 tx_buffer_tail[MIOS32_UART_NUM];
static volatile u8 tx_buffer_head[MIOS32_UART_NUM];
static volatile u8 tx_buffer_size[MIOS32_UART_NUM];
#endif


/////////////////////////////////////////////////////////////////////////////
// Initializes UART interfaces
// IN: <mode>: currently only mode 0 supported
// OUT: returns < 0 if initialisation failed
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_Init(u32 mode)
{
  GPIO_InitTypeDef GPIO_InitStructure;

  // currently only mode 0 supported
  if( mode != 0 )
    return -1; // unsupported mode

#if MIOS32_UART_NUM == 0
  return -1; // no UARTs
#else

  // map UART pins
  MIOS32_UART0_REMAP_FUNC;
#if MIOS32_UART_NUM >= 2
  MIOS32_UART1_REMAP_FUNC;
#endif

  // configure UART pins
  GPIO_StructInit(&GPIO_InitStructure);
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;

  // outputs as push-pull
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_InitStructure.GPIO_Pin = MIOS32_UART0_TX_PIN;
  GPIO_Init(MIOS32_UART0_TX_PORT, &GPIO_InitStructure);
#if MIOS32_UART_NUM >= 2
  GPIO_InitStructure.GPIO_Pin = MIOS32_UART1_TX_PIN;
  GPIO_Init(MIOS32_UART1_TX_PORT, &GPIO_InitStructure);
#endif

  // inputs with internal pull-up
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
  GPIO_InitStructure.GPIO_Pin = MIOS32_UART0_RX_PIN;
  GPIO_Init(MIOS32_UART0_RX_PORT, &GPIO_InitStructure);
#if MIOS32_UART_NUM >= 2
  GPIO_InitStructure.GPIO_Pin = MIOS32_UART1_RX_PIN;
  GPIO_Init(MIOS32_UART1_RX_PORT, &GPIO_InitStructure);
#endif

  // enable all USART clocks
  // TODO: more generic approach for different UART selections
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2 | RCC_APB1Periph_USART3, ENABLE);

  // USART configuration
  USART_InitTypeDef USART_InitStructure;
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  USART_InitStructure.USART_Parity = USART_Parity_No;
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

  USART_InitStructure.USART_BaudRate = MIOS32_UART0_BAUDRATE;
  USART_Init(MIOS32_UART0, &USART_InitStructure);
#if MIOS32_UART_NUM >=2
  USART_InitStructure.USART_BaudRate = MIOS32_UART1_BAUDRATE;
  USART_Init(MIOS32_UART1, &USART_InitStructure);
#endif

  // configure and enable UART interrupts
  // TODO: review interrupt priorities!
  NVIC_InitTypeDef NVIC_InitStructure;

  NVIC_InitStructure.NVIC_IRQChannel = MIOS32_UART0_IRQ_CHANNEL;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
  USART_ITConfig(MIOS32_UART0, USART_IT_RXNE, ENABLE);

#if MIOS32_UART_NUM >= 2
  NVIC_InitStructure.NVIC_IRQChannel = MIOS32_UART1_IRQ_CHANNEL;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
  USART_ITConfig(MIOS32_UART1, USART_IT_RXNE, ENABLE);
#endif

  // clear buffer counters
  int i;
  for(i=0; i<MIOS32_UART_NUM; ++i) {
    rx_buffer_tail[i] = rx_buffer_head[i] = rx_buffer_size[i] = 0;
    tx_buffer_tail[i] = tx_buffer_head[i] = tx_buffer_size[i] = 0;
  }

  // enable UARTs
  USART_Cmd(MIOS32_UART0, ENABLE);
#if MIOS32_UART_NUM >= 2
  USART_Cmd(MIOS32_UART1, ENABLE);
#endif

  return 0; // no error
#endif
}


/////////////////////////////////////////////////////////////////////////////
// returns number of free bytes in receive buffer
// IN: UART number (0..1)
// OUT: number of free bytes
//      if uart not available: 0
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_RxBufferFree(u8 uart)
{
#if MIOS32_UART_NUM == 0
  return 0; // no UART available
#else
  if( uart >= MIOS32_UART_NUM )
    return 0;
  else
    return MIOS32_UART_RX_BUFFER_SIZE - rx_buffer_size[uart];
#endif
}


/////////////////////////////////////////////////////////////////////////////
// returns number of used bytes in receive buffer
// IN: UART number (0..1)
// OUT: number of used bytes
//      if uart not available: 0
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_RxBufferUsed(u8 uart)
{
#if MIOS32_UART_NUM == 0
  return 0; // no UART available
#else
  if( uart >= MIOS32_UART_NUM )
    return 0;
  else
    return rx_buffer_size[uart];
#endif
}


/////////////////////////////////////////////////////////////////////////////
// gets a byte from the receive buffer
// IN: UART number (0..1)
// OUT: -1 if UART not available
//      -2 if no new byte available
//      otherwise received byte
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_RxBufferGet(u8 uart)
{
#if MIOS32_UART_NUM == 0
  return -1; // no UART available
#else
  if( uart >= MIOS32_UART_NUM )
    return -1; // UART not available

  if( !rx_buffer_size[uart] )
    return -2; // nothing new in buffer

  // get byte - this operation should be atomic!
  vPortEnterCritical(); // port specific FreeRTOS function to disable IRQs (nested)
  u8 b = rx_buffer[uart][rx_buffer_tail[uart]];
  if( ++rx_buffer_tail[uart] >= MIOS32_UART_RX_BUFFER_SIZE )
    rx_buffer_tail[uart] = 0;
  --rx_buffer_size[uart];
  vPortExitCritical(); // port specific FreeRTOS function to enable IRQs (nested)

  return b; // return received byte
#endif
}


/////////////////////////////////////////////////////////////////////////////
// returns the next byte of the receive buffer without taking it
// IN: UART number (0..1)
// OUT: -1 if UART not available
//      -2 if no new byte available
//      otherwise received byte
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_RxBufferPeek(u8 uart)
{
#if MIOS32_UART_NUM == 0
  return -1; // no UART available
#else
  if( uart >= MIOS32_UART_NUM )
    return -1; // UART not available

  if( !rx_buffer_size[uart] )
    return -2; // nothing new in buffer

  // get byte - this operation should be atomic!
  vPortEnterCritical(); // port specific FreeRTOS function to disable IRQs (nested)
  u8 b = rx_buffer[uart][rx_buffer_tail[uart]];
  vPortExitCritical(); // port specific FreeRTOS function to enable IRQs (nested)

  return b; // return received byte
#endif
}


/////////////////////////////////////////////////////////////////////////////
// puts a byte onto the receive buffer
// IN: UART number (0..1) and byte to be sent
// OUT: 0 if no error
//      -1 if UART not available
//      -2 if buffer full (retry)
//      
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_RxBufferPut(u8 uart, u8 b)
{
#if MIOS32_UART_NUM == 0
  return -1; // no UART available
#else
  if( uart >= MIOS32_UART_NUM )
    return -1; // UART not available

  if( rx_buffer_size[uart] >= MIOS32_UART_RX_BUFFER_SIZE )
    return -2; // buffer full (retry)

  // copy received byte into receive buffer
  // this operation should be atomic!
  vPortEnterCritical(); // port specific FreeRTOS function to disable IRQs (nested)
  rx_buffer[uart][rx_buffer_head[uart]] = b;
  if( ++rx_buffer_head[uart] >= MIOS32_UART_RX_BUFFER_SIZE )
    rx_buffer_head[uart] = 0;
  ++rx_buffer_size[uart];
  vPortExitCritical(); // port specific FreeRTOS function to enable IRQs (nested)

  return 0; // no error
#endif
}


/////////////////////////////////////////////////////////////////////////////
// returns number of free bytes in transmit buffer
// IN: UART number (0..1)
// OUT: number of free bytes
//      if uart not available: 0
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_TxBufferFree(u8 uart)
{
#if MIOS32_UART_NUM == 0
  return 0; // no UART available
#else
  if( uart >= MIOS32_UART_NUM )
    return 0;
  else
    return MIOS32_UART_TX_BUFFER_SIZE - tx_buffer_size[uart];
#endif
}


/////////////////////////////////////////////////////////////////////////////
// returns number of used bytes in transmit buffer
// IN: UART number (0..1)
// OUT: number of used bytes
//      if uart not available: 0
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_TxBufferUsed(u8 uart)
{
#if MIOS32_UART_NUM == 0
  return 0; // no UART available
#else
  if( uart >= MIOS32_UART_NUM )
    return 0;
  else
    return tx_buffer_size[uart];
#endif
}


/////////////////////////////////////////////////////////////////////////////
// gets a byte from the transmit buffer
// IN: UART number (0..1)
// OUT: -1 if UART not available
//      -2 if no new byte available
//      otherwise transmitted byte
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_TxBufferGet(u8 uart)
{
#if MIOS32_UART_NUM == 0
  return -1; // no UART available
#else
  if( uart >= MIOS32_UART_NUM )
    return -1; // UART not available

  if( !tx_buffer_size[uart] )
    return -2; // nothing new in buffer

  // get byte - this operation should be atomic!
  vPortEnterCritical(); // port specific FreeRTOS function to disable IRQs (nested)
  u8 b = tx_buffer[uart][tx_buffer_tail[uart]];
  if( ++tx_buffer_tail[uart] >= MIOS32_UART_TX_BUFFER_SIZE )
    tx_buffer_tail[uart] = 0;
  --tx_buffer_size[uart];
  vPortExitCritical(); // port specific FreeRTOS function to enable IRQs (nested)

  return b; // return transmitted byte
#endif
}


/////////////////////////////////////////////////////////////////////////////
// puts more than one byte onto the transmit buffer (used for atomic sends)
// IN: UART number (0..1), buffer to be sent and buffer length
// OUT: 0 if no error
//      -1 if UART not available
//      -2 if buffer full or cannot get all requested bytes (retry)
//      -3 if UART not supported by MIOS32_UART_TxBufferPut Routine
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_TxBufferPutMore(u8 uart, u8 *buffer, u16 len)
{
#if MIOS32_UART_NUM == 0
  return -1; // no UART available
#else
  if( uart >= MIOS32_UART_NUM )
    return -1; // UART not available

  if( tx_buffer_size[uart] >= (MIOS32_UART_TX_BUFFER_SIZE-len+1) )
    return -2; // buffer full or cannot get all requested bytes (retry)

  // copy bytes to be transmitted into transmit buffer
  // this operation should be atomic!
  vPortEnterCritical(); // port specific FreeRTOS function to disable IRQs (nested)
  u16 i;
  for(i=0; i<len; ++i) {
    tx_buffer[uart][tx_buffer_head[uart]] = *buffer++;

    if( ++tx_buffer_head[uart] >= MIOS32_UART_TX_BUFFER_SIZE )
      tx_buffer_head[uart] = 0;

    // enable Tx interrupt if buffer was empty
    if( ++tx_buffer_size[uart] == 1 ) {
      switch( uart ) {
        case 0: USART_ITConfig(MIOS32_UART0, USART_IT_TXE, ENABLE); break;
        case 1: USART_ITConfig(MIOS32_UART1, USART_IT_TXE, ENABLE); break;
        default: return -3; // uart not supported by routine (yet)
      }
    }
  }
  vPortExitCritical(); // port specific FreeRTOS function to enable IRQs (nested)

  return 0; // no error
#endif
}


/////////////////////////////////////////////////////////////////////////////
// puts a byte onto the transmit buffer
// IN: UART number (0..1) and byte to be sent
// OUT: 0 if no error
//      -1 if UART not available
//      -2 if buffer full (retry)
//      -3 if UART not supported by MIOS32_UART_TxBufferPut Routine
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_TxBufferPut(u8 uart, u8 b)
{
  // for more comfortable usage...
  // -> just forward to MIOS32_UART_TxBufferPutMore
  return MIOS32_UART_TxBufferPutMore(uart, &b, 1);
}


/////////////////////////////////////////////////////////////////////////////
// Interrupt handler for first UART
/////////////////////////////////////////////////////////////////////////////
#if MIOS32_UART_NUM >= 1
MIOS32_UART0_IRQHANDLER_FUNC
{
  if( USART_GetITStatus(MIOS32_UART0, USART_IT_RXNE) != RESET ) {
    if( MIOS32_UART_RxBufferPut(0, USART_ReceiveData(MIOS32_UART0)) < 0 ) {
      // here we could add some error handling
    }
  }
  
  if( USART_GetITStatus(USART1, USART_IT_TXE) != RESET ) {
    if( MIOS32_UART_TxBufferUsed(0) > 0 ) {
      s32 b = MIOS32_UART_TxBufferGet(0);
      if( b < 0 ) {
	// here we could add some error handling
	USART_SendData(MIOS32_UART0, 0xff);
      } else {
	USART_SendData(MIOS32_UART0, (u8)b);
      }
    } else {
      USART_ITConfig(MIOS32_UART0, USART_IT_TXE, DISABLE);
    }
  }
}
#endif


/////////////////////////////////////////////////////////////////////////////
// Interrupt handler for second UART
/////////////////////////////////////////////////////////////////////////////
#if MIOS32_UART_NUM >= 2
MIOS32_UART1_IRQHANDLER_FUNC
{
  if( USART_GetITStatus(MIOS32_UART1, USART_IT_RXNE) != RESET ) {
    if( MIOS32_UART_RxBufferPut(1, USART_ReceiveData(MIOS32_UART1)) < 0 ) {
      // here we could add some error handling
    }
  }
  
  if( USART_GetITStatus(USART1, USART_IT_TXE) != RESET ) {
    if( MIOS32_UART_TxBufferUsed(1) > 0 ) {
      s32 b = MIOS32_UART_TxBufferGet(1);
      if( b < 0 ) {
	// here we could add some error handling
	USART_SendData(MIOS32_UART1, 0xff);
      } else {
	USART_SendData(MIOS32_UART1, (u8)b);
      }
    } else {
      USART_ITConfig(MIOS32_UART1, USART_IT_TXE, DISABLE);
    }
  }
}
#endif


#endif /* MIOS32_DONT_USE_UART */