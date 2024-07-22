FROM ps2dev/ps2dev:latest
RUN apk update
RUN apk add bash build-base git nano make mpc mpc1 mpfr4 python3 py3-pip gmp wget zip


WORKDIR /project
COPY ./project/* .

