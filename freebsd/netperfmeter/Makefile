# Created by: Thomas Dreibholz <thomas.dreibholz@gmail.com>

PORTNAME=	netperfmeter
PORTVERSION=	1.9.4~rc0
CATEGORIES=	benchmarks
MASTER_SITES=	https://www.nntb.no/~dreibh/netperfmeter/download/

MAINTAINER=	thomas.dreibholz@gmail.com
COMMENT=	Network Performance Meter
WWW=            https://www.nntb.no/~dreibh/netperfmeter/

LICENSE=	GPLv3+
LICENSE_FILE=	${WRKSRC}/COPYING

USES=		cmake compiler:c11 shebangfix tar:xz

SHEBANG_FILES=	src/pdfembedfonts src/setpdfmetadata src/plot-netperfmeter-results

CMAKE_ARGS=	-DCMAKE_INSTALL_MANDIR=${PREFIX}/man
CMAKE_OFF=	WITH_NEAT BUILD_TEST_PROGRAMS
CMAKE_ON=	BUILD_PLOT_PROGRAMS

.include <bsd.port.mk>
