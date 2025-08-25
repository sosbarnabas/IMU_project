#include "MultiPortExoMotors.h"
#include <iostream>
#include <vector>
#include <thread>
#include <QCoreApplication>
int main(int argc, char *argv[]){
    QCoreApplication app(argc, argv);

    std::vector<int> fn3;
    for (int i = 0; i < 360; i++) {
        if (i < 100) {
            fn3.push_back(110);
        }
        else if (i >= 100 && i < 180) {
            fn3.push_back((110-(i-100)));
        }
        else {
            fn3.push_back(30);
        }
    }


    std::vector<std::string> ports = {"COM3"};
    exoskeleton::core::MultiPortExoMotors controller(ports, 3.0);
    controller.connect();
    controller.raw_enable(0);

    //controller.set_function(0,fn3, 7);
    //controller.select_function(0,7);
        for (int i = 0 ; i < 200 ; i++) {
            if (i == 100) controller.set_zero(0);
            auto datas = controller.read();
            for (SingleMotorData const& data : datas ) {
                std::cout << data << std::endl;
            }
        }

    controller.disable(0);
    return app.exec();
}
