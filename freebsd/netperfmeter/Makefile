PORTNAME=	netperfmeter
DISTVERSION=	2.0.0~rc1.1
PORTREVISION=	1
CATEGORIES=	benchmarks
MASTER_SITES=	https://www.nntb.no/~dreibh/netperfmeter/download/

MAINTAINER=	dreibh@simula.no
COMMENT=	Network Performance Meter
WWW=		https://www.nntb.no/~dreibh/netperfmeter/

LICENSE=	GPLv3+
LICENSE_FILE=	${WRKSRC}/COPYING

BUILD_DEPENDS=	gm:graphics/GraphicsMagick \
		pdf2svg:graphics/pdf2svg \
		gs:print/ghostscript10

USES=		cmake compiler:c11 desktop-file-utils shebangfix tar:xz

SHEBANG_FILES=	src/pdfembedfonts src/setpdfmetadata src/plot-netperfmeter-results

CMAKE_OFF=	WITH_TEST_PROGRAMS
CMAKE_ON=	WITH_PLOT_PROGRAMS WITH_ICONS

.include <bsd.port.mk>
