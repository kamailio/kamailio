
loadmodule "app_java.so"
modparam("app_java", "script_name", "/opt/kamailio/java/Kamailio.class")
modparam("app_java", "pkg_tree_path", "")
modparam("app_java", "class_name", "Kamailio")
#modparam("app_java", "java_options", "-Xdebug -Djava.compiler=NONE -Djava.class.path=/opt/kamailio/lib/kamailio/modules:/opt/kamailio/java  -Djava.library.path=/usr/lib/jvm/java-gcj-4.7:/opt/kamailio/lib/kamailio/modules:/opt/kamailio/java -verbose:gc,class,jni")
#modparam("app_java", "java_options", "-Xdebug -Djava.compiler=NONE -Djava.class.path=/opt/kamailio/lib/kamailio/modules:/opt/kamailio/java  -verbose:gc,class,jni")
modparam("app_java", "java_options", "-Djava.compiler=NONE -Djava.class.path=/opt/kamailio/lib/kamailio/modules:/opt/kamailio/java  -verbose:gc,jni")
modparam("app_java", "child_init_method", "child_init")

