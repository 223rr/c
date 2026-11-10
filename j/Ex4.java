package j; 
//不能上课异常
class NoClassException extends Exception {
    public NoClassException(String message) {
        super(message);
    }
}
//蓝屏异常
class LanPingException extends Exception {
    public LanPingException(String message) {
        super(message);
    }
}
class Computer {
    int status = 1;//假设 1 和 2 都代表蓝屏异常状态
    public void run() throws LanPingException {
        if (status == 1 || status == 2)// 如果状态是1或2，抛出蓝屏异常
        {
            throw new LanPingException("电脑蓝屏异常");
        }
        System.out.println("电脑正常运行");
    }
    public void restart() //重启电脑
    {
        System.out.println("尝试重启电脑...");
        status = 1;
    }
}
class Teacher {
    String name;
    Computer comp;
    public Teacher(String name, Computer comp) {
        this.name = name;
        this.comp = comp;
    }
    public void test() {
        System.out.println("学生正在做练习");
    }
    public void teach() throws NoClassException {
        try {
            comp.run();
            System.out.println(name + " 正在使用电脑上课");
        } catch (LanPingException e) {
            System.out.println("发生异常：" + e.getMessage());
            comp.restart();
            try {
                comp.run();
                System.out.println(name + " 重启后继续上课");
            } catch (LanPingException ex) // 如果重启后依然报错，异常交给主方法处理
            {
                throw new NoClassException("抛出不能上课的异常 (处理：自习或放假)");
            }
        }
    }
}
//测试
class ExceptionTeach { 
    public static void main(String[] args) {
        Computer my = new Computer();
        Teacher teacher = new Teacher("张老师", my);
        try {
            teacher.teach();
        } catch (NoClassException e) {
            System.out.println(e.getMessage());
            teacher.test();
        }
    }
}
