
#JAVA_HOME=$(readlink -f $(which javac) | sed "s:bin/javac::")
#CLASSPATH := /usr/local/lib/kamailio/modules

JAVA_HOME ?= /usr/lib/jvm/java-gcj-4.7/

all:
#	export JAVA_HOME=/usr/lib/jvm/java-gcj-4.7:/opt/kamailio/lib/kamailio/modules:/opt/kamailio/java
#	export CLASSPATH=/opt/kamailio/lib/kamailio/modules:/opt/kamailio/java
	javac Kamailio.java

clean:
	rm -f *.class



