### Created by Dr. Benjamin Richards (b.richards@qmul.ac.uk) 

### Download base image from cern repo on docker hub
FROM wcsim/wcsim:base

### Run the following commands as super user (root):
USER root

### Get and build WCSim
WORKDIR $HYPERKDIR
COPY . WCSim
RUN mkdir $HYPERKDIR/WCSim-build $HYPERKDIR/WCSim-install
WORKDIR $HYPERKDIR/WCSim-build
RUN source $HYPERKDIR/env-WCSim.sh &&\
    cmake3 $HYPERKDIR/WCSim &&\
    make -j`nproc` install

### Open terminal
ENTRYPOINT source $HYPERKDIR/env-WCSim.sh && /bin/bash
 
