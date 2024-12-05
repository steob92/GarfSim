FROM rootproject/root:6.32.04-ubuntu24.04


ENV GARFIELD_HOME=/opt/garfieldpp

WORKDIR /opt/

RUN git clone https://gitlab.cern.ch/garfield/garfieldpp.git && \
    cd garfieldpp && mkdir build && cd build && \
    cmake -DCMAKE_INSTALL_PREFIX=${GARFIELD_HOME}/install .. && \
    make -j $(nproc) && make install && \
    mkdir ${GARFIELD_HOME}/install/lib64/ && \
    ln -s ${GARFIELD_HOME}/install/lib/* ${GARFIELD_HOME}/install/lib64/ 

ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${GARFIELD_HOME}/install/lib

CMD [ "bash" ]