
#include <iostream>

#include <boost/asio.hpp>



using namespace boost::asio;
using namespace boost::asio::ip;

int main()
{
    try
    {
        boost::asio::io_service io; //构造IO服务,由于非异步,无需run
    
        udp::socket socket(io, udp::endpoint(udp::v4(), 11091));//构造socket并绑定到1024端口
    
        for (;;)
        {
            std::array<char, 1024> recv_buf;//接收缓冲
            udp::endpoint remote_endpoint; //发送端信息
            boost::system::error_code error;
            
            //阻塞读取
            auto size = socket.receive_from(boost::asio::buffer(recv_buf), remote_endpoint, 0, error);
    
            if (error && error != boost::asio::error::message_size)
            {
                throw boost::system::system_error(error);
            }
            std::cout.write(recv_buf.data(),size);//输出结果
        }
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}