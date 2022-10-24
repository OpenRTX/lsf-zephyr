.. _UART driver polling samples:

UART driver configure samples
###########

Overview
********
1. 该例程先打印控制台串口当前的配置信息，包括波特率、数据位、停止位、奇偶为、流控模式，
2. 6秒过后，程序将会把串口的波特率动态的改变为9600，请在此期间，调整接收端的波特率为9600以确保
能正确的接收到日志，
3. 之后将会以9600的波特率一直打印`hello_world`
