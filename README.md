完成实验的代码全在libsponge目录下

## Lab0
webget
直接写了一个“webget”程序，创建一个TCP stream socket，去和一个web server建立连接。
发送请求并且返回请求
要求实现get_URL函数，功能为向指定IP地址发送HTTP GET请求，然后输出所有响应。
byte_stream
要求实现一个有序字节流类（in-order byte stream），使之支持读写、容量控制。这个字节流类似于一个带容量的队列，从一头读，从另一头写。当流中的数据达到容量上限时，便无法再写入新的数据。

手写了一个队列模拟字节流
## Lab1  stream_reassembler
TCP接收方的重新排序器 
用的是set 用map之类的也可以
- 对于收到的字节流进行判断 提前到达或者延迟到达的不接受
- 如果有一部分是在排序器里面都需要更新
- 插入排序器后 需要对合并子串 运用set自带的lowerbound快速确定插入位置，前后重复比较，用个自己写的子函数判断重叠的字顺便合并之。
[图片]
## Lab2 
wrapping_integers
实现接收端
实现序列号、绝对序列号与流索引间的转换
[图片]
[图片]
需要提一下的地方有checkpoint表示最近一次转换求得的absolute seqno
tcp_receiver
整合前面的stream_reassembler  byte_stream
## Lab3 
tcp_sender
实现了累计确认机制，使得发送方可以更方便地进行超时重传和快速重传。通过累计确认，有效地处理丢失的数据包，提高 了数据传输的可靠性和效率
LAB3是关于Sender的。Sender要做的事情比较多。
- fill_window()：根据window_size，如果窗口没填满，并且发送端有数据需要发送，就可以使用fill_window发送segment。
- ack_received(ackno, window_size)：  更新Sender自身的window_size状态。  累计确认ackno
- ticks(ms_since_last_tick)   主要实现超时重传

## 心跳包
这里主要Sender的window_size要设置
  // take window_size as 1 when it equal 0
  // 新跳包  如果接收方窗口满了 你这里也是0 不发送东西的话,
  // 那么接收方永远不会发送ack 就卡死了 所以必须发送一字节的内容
## Lab4 TCPConnection
封装TCPSender和TCPReceiver
构建TCP的有限状态机（FSM）  主要就是模拟tcp的每一个状态
## Lab5 network_interface
实现一个ARP协议
其实和之前的差不多 重点是搞懂EthernetFrame类各个成员变量
## Lab6 router
这节要求实现基于最长前缀匹配规则的路由器转发功能