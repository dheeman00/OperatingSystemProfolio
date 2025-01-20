#!/bin/sh
USER=$1

# Start by cleaning the execution environment
rm -f ~/.ishrc

touch ${USER}.out
echo "*** Compile Process ***" > ${USER}.out
( cd ${USER}; 
rm ish *.o;
make clean;
make;
ls -l ish) >> $(USER}.out

echo "*** Basic Testcases ***" >> ${USER}.out
(cd ${USER}; ./ish < ../testcases/basic.ish) >> ${USER}.out

echo "*** Environment Testcases ***" >> ${USER}.out
(cd ${USER}; ./ish < ../testcases/environment.ish) >> ${USER}.out

echo "*** Aliasing Testcases ***" >> ${USER}.out
(cd ${USER}; ./ish < ../testcases/alias.ish) >> ${USER}.out

echo "*** (Process, File Descriptor, Memory) Leak Testcases ***" >> ${USER}.out
(cd ${USER}; ./ish < ../testcases/leak.ish) >> ${USER}.out

echo "*** ISHRC Testcases ***" >> ${USER}.out
(cd ${USER}; sh ../testcases/ishrc.sh) >> ${USER}.out

echo "*** Source Code Check ***">> ${USER}.out
(cd ${USER}; sh ../testcases/checksource.sh ) >> ${USER}.out

echo "*** Redirection Testcases ***" >> ${USER}.out
(cd ${USER}; ./ish < ../testcases/redirection.ish) >> ${USER}.out

echo "*** Job Control Testcases ***" >> ${USER}.out
(cd ${USER}; ./ish < ../testcases/jobctl.ish) >> ${USER}.out

echo "*** Pipeline Testcases ***" >> ${USER}.out
(cd ${USER}; ./ish < ../testcases/pipelines.ish) >> ${USER}.out

