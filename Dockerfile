FROM debian:bookworm-slim

ARG USERNAME=adnan
ARG USER_UID=1000
ARG USER_GID=$USER_UID

RUN groupadd --gid $USER_GID $USERNAME \
&& useradd --uid $USER_UID --gid $USER_GID -m $USERNAME

RUN dpkg --add-architecture i386

RUN apt-get update && apt-get install -y \
    bash \
    curl \
    build-essential \
    git \
    nano \
    make \
    libmpc-dev \
    libmpfr-dev \
    libgmp-dev \
    python3 \
    python3-pip \
    wget \
    zip \
    libstdc++5:i386 \
    libc6:i386 \
    libstdc++6:i386 \
    libgcc-s1:i386 \
    genisoimage \
    && rm -rf /var/lib/apt/lists/*


ENV PS2DEV=/usr/local/ps2dev
RUN mkdir -p $PS2DEV
RUN chown -R $USER: $PS2DEV
ENV PS2SDK=$PS2DEV/ps2sdk
ENV GSKIT=$PS2DEV/gsKit
ENV PATH=$PATH:${PS2DEV}/bin:${PS2DEV}/ee/bin:${PS2DEV}/iop/bin:${PS2DEV}/dvp/bin:${PS2SDK}/bin

#pull latest ps2dev ubuntu image
RUN curl -o ps2dev-latest.tar.gz -LC - https://github.com/ps2dev/ps2dev/releases/download/latest/ps2dev-$(if [[ "$OSTYPE" == "darwin"* ]]; then echo macos; else echo ubuntu; fi)-latest.tar.gz
RUN tar -xf ps2dev-latest.tar.gz --strip-components 1 -C $PS2DEV

RUN ls -la /usr/local/ps2dev

# build libraries
WORKDIR /dependencies
RUN mkdir -p lib
#lib will be copied in a post build hook
RUN git clone https://github.com/nothings/stb.git
WORKDIR /dependencies/stb
RUN printf '#define STB_IMAGE_IMPLEMENTATION\n#include "stb_image.h"\n' \
  | mips64r5900el-ps2-elf-g++ -O2 -G0 -I. -x c++ -c - -o stb_image.o
RUN mips64r5900el-ps2-elf-ar cru libstb_image.a stb_image.o
RUN cp libstb_image.a ../lib/


WORKDIR /dependencies
RUN git clone https://github.com/tinyobjloader/tinyobjloader.git
WORKDIR /dependencies/tinyobjloader
# patch the IEE-754 float requirement, PS2 doesn't support that
RUN sed -i '/static_assert(std::numeric_limits<float>::is_iec559/,/);/d' tiny_obj_loader.h
RUN mips64r5900el-ps2-elf-g++ -O2 -G0 -I. -x c++ -c tiny_obj_loader.cc -o tiny_obj_loader.o
RUN mips64r5900el-ps2-elf-ar cru libtiny_obj_loader.a tiny_obj_loader.o
RUN cp libtiny_obj_loader.a ../lib/

WORKDIR /dependencies
RUN git clone https://github.com/lvandeve/lodepng.git
WORKDIR /dependencies/lodepng
RUN mips64r5900el-ps2-elf-g++ -O2 -G0 -I. -x c++ -c lodepng.cpp -o lodepng.o
RUN mips64r5900el-ps2-elf-ar cru liblodepng.a lodepng.o
RUN cp liblodepng.a ../lib/

#build tools
WORKDIR /dependencies
RUN git clone https://github.com/glampert/vclpp.git
WORKDIR /dependencies/vclpp
RUN g++ vclpp_main.cpp -o vclpp
RUN cp vclpp /usr/local/bin/

WORKDIR /dependencies
RUN curl -L -O "https://ps2linux.no-ip.info/playstation2-linux.com/download/vcl/vcl.1.4.beta7.x86.tar"
RUN tar -xf vcl.1.4.beta7.x86.tar
RUN cp vcl.1.4.beta7.x86/vcl_14beta7_x86 /usr/local/bin/vcl
