#! /bin/bash
set -e

echo "################################################################################################"
echo "# ppddl-planner: Copyright Florent Teichteil-Koenigsbuch and Guillaume Infantes and Ugur Kuter #"
echo "################################################################################################"

echo -e "\e[31mInstall ppddl-planner under a specific prefix on disk (for make install) or keep it local under the source directory (no make install) [prefix|local]?\e[0m"
read installpolicy
PREFIX=""
if [ "$installpolicy" == "prefix" ]; then
    echo -e "\e[31mEnter the prefix where you want to install ppddl-planner\e[0m"
    read pfx
    PREFIX=$(readlink -f ${pfx})
    echo "ppddl-planner will be installed in $PREFIX (you will have to explicitly 'make install' in case you need sudo privileges)"
elif [ "$installpolicy" == "local" ]; then
    PREFIX=$(readlink -f ${PWD})
    echo "ppddl-planner compilation files will be located under $PREFIX"
else
    echo -e "\e[31mUnexpected choice, exiting now\e[0m"
    exit 1
fi

CUDD="sdk/cudd-2.4.1-ftk"
MDPSIM="sdk/mdpsim-2.2-ftk"
FF="sdk/FF-v2.3-ftk"
MFF="sdk/Metric-FF-ftk"

echo -e "\e[31mHAVE YOU CHECKED $CUDD/Makefile ? [yes|no]\e[0m"
read cuddcheck
if [ "$cuddcheck" != "yes" ]; then
    echo -e "\e[31mPlease edit and check $CUDD/Makefile before compiling ppddl-planner\e[0m"
    exit 1
fi

echo -e "\e[31mHow many CPUs would you like to use to compile the ppddl-planner?\e[0m"
read numcpu
if ! (( $numcpu > 0 )); then
    echo -e "\e[31mThe number of CPUs must be a positive integer\e[0m"
    exit 1
fi

echo "Entering directory $CUDD"
cd $CUDD
make -j$(($numcpu + 1))
cd ../..
echo "Leaving directory $CUDD"

echo "Entering directory $MDPSIM"
cd $MDPSIM
autoreconf -f -i
./configure --prefix=$PWD --disable-shared --enable-static CFLAGS="-O3 -DNDEBUG" CXXFLAGS="-O3 -DNDEBUG"
make -j$(($numcpu + 1))
make install
cd ../..
echo "Leaving directory $MDPSIM"

pass=""
if [ "$installpolicy" == "prefix" ]; then
    if [ ! -w $PREFIX ] ; then
        echo -en "\e[31mEnter password for sudo rights:\e[0m"
        read -s pass
    fi
fi

echo "Entering directory $FF"
cd $FF
make -j$(($numcpu + 1))
FFCOMMAND=""
if [ "$installpolicy" == "prefix" ]; then
    if [ -w $PREFIX ] ; then
        mkdir -p $PREFIX/bin
        cp ppddl_planner_ff $PREFIX/bin
        chmod a+x $PREFIX/bin/ppddl_planner_ff
    else
        echo $pass | sudo -S mkdir -p $PREFIX/bin
        echo $pass | sudo -S cp ppddl_planner_ff $PREFIX/bin
        echo $pass | sudo -S chmod a+x $PREFIX/bin/ppddl_planner_ff
    fi
    FFCOMMAND="ppddl_planner_ff"
else
    FFCOMMAND=$(readlink -f ppddl_planner_ff)
fi
cd ../..
echo "Leaving directory $FF"

echo "Entering directory $MFF"
cd $MFF
make -j$(($numcpu + 1))
MFFCOMMAND=""
if [ "$installpolicy" == "prefix" ]; then
    if [ -w $PREFIX ] ; then
        mkdir -p $PREFIX/bin
        cp ppddl_planner_mff $PREFIX/bin
        chmod a+x $PREFIX/bin/ppddl_planner_mff
    else
        echo $pass | sudo -S mkdir -p $PREFIX/bin
        echo $pass | sudo -S cp ppddl_planner_mff $PREFIX/bin
        echo $pass | sudo -S chmod a+x $PREFIX/bin/ppddl_planner_mff
    fi
    MFFCOMMAND="ppddl_planner_mff"
else
    MFFCOMMAND=$(readlink -f ppddl_planner_mff)
fi
cd ../..
echo "Leaving directory $MFF"

echo "Compiling ppddl-planner"
echo -e "\e[31mEnter the python version for which you want to compile the python wrapper ('none' if you do not want to compile the python bindings):\e[0m"
read pythoncompatibility
autoreconf -f -i
./configure --prefix=$PREFIX --with-cudd-prefix=$CUDD --with-mdpsim-prefix=$MDPSIM --with-ff-command=$FFCOMMAND --with-mff-command=$MFFCOMMAND CFLAGS="-O3 -DNDEBUG" CXXFLAGS="-O3 -DNDEBUG" PYTHON_VERSION="$pythoncompatibility"
make -j$(($numcpu + 1))

if [ "$installpolicy" == "prefix" ]; then
    if [ ! -w $PREFIX ] ; then
        echo $pass | sudo -S make install
    else
        make install
    fi
elif [ "$installpolicy" == "local" ]; then
    make install
fi
