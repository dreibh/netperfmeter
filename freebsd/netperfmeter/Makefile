# Created by: Thomas Dreibholz <dreibh@iem.uni-due.de>
# $FreeBSD: head/benchmarks/netperfmeter/Makefile 429903 2016-12-29 12:50:33Z amdmi3 $

PORTNAME=	netperfmeter
PORTVERSION=	1.8.3
CATEGORIES=	benchmarks
MASTER_SITES=	https://www.uni-due.de/~be0001/netperfmeter/download/
EXTRACT_SUFX=	.tar.xz

MAINTAINER=	dreibh@iem.uni-due.de
COMMENT=	Network Performance Meter

LICENSE=	GPLv3+
LICENSE_FILE=	${WRKSRC}/COPYING

GNU_CONFIGURE=	yes
USES=		cmake shebangfix
CMAKE_ARGS+=	-DCMAKE_INSTALL_MANDIR=${PREFIX}/man -DWITH_NEAT=0 -DBUILD_TEST_PROGRAMS=0 -DBUILD_PLOT_PROGRAMS=1
SHEBANG_FILES=	src/pdfembedfonts src/pdfmetadata src/plot-netperfmeter-results

.include <bsd.port.mk>
