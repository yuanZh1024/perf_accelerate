Maven 的 `compile` 是构建生命周期阶段；protobuf-maven-plugin 挂在这个阶段上，在编译过程中调用 `protoc`，把 `.proto` 转成 Java。

 protoc 可以理解成“Protobuf 的编译器”。


                   user.proto
                      │
                      │ Schema
                      ↓
                   protoc
                      │
                      ↓
              UserOuterClass.java
                      │
                      ↓
                 Java对象
                      │
                      │ toByteArray()
                      ↓
                    bytes
                      │
                      │ parseFrom()
                      ↓
                 Java对象



protoc --java_out=. user.proto
UserOuterClass.java 里面有 
 - 构造函数
 - 怎么序列化、反序列化 user.proto  
 - getter setter


 