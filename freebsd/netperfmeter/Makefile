PORTNAME=	netperfmeter
DISTVERSION=	2.0.6~rc0
CATEGORIES=	benchmarks
MASTER_SITES=	https://www.nntb.no/~dreibh/netperfmeter/download/

MAINTAINER=	dreibh@simula.no
COMMENT=	Network Performance Meter
WWW=		https://www.nntb.no/~dreibh/netperfmeter/

LICENSE=	GPLv3+
LICENSE_FILE=	${WRKSRC}/COPYING

USES=		cmake shebangfix tar:xz

SHEBANG_FILES=	src/pdfembedfonts src/plot-netperfmeter-results \
		src/setpdfmetadata

OPTIONS_DEFINE=			ICONS \
				PLOT_PROGRAMS \
				EXAMPLE_SCRIPTS \
				EXAMPLE_RESULTS

OPTIONS_DEFAULT=		${OPTIONS_DEFINE}
OPTIONS_SUB=			yes

ICONS_DESC=			Include icons
PLOT_PROGRAMS_DESC=		Include plot programs
EXAMPLE_SCRIPTS_DESC=		Include example scripts
EXAMPLE_RESULTS_DESC=		Include example results

ICONS_CMAKE_BOOL=		WITH_ICONS
ICONS_BUILD_DEPENDS=		gm:graphics/GraphicsMagick \
				mutool:graphics/mupdf
ICONS_USES=			ghostscript shared-mime-info

PLOT_PROGRAMS_CMAKE_BOOL=	WITH_PLOT_PROGRAMS
PLOT_PROGRAMS_RUN_DEPENDS=	R-cran-bit64>0:devel/R-cran-bit64 \
				R-cran-data.table>0:devel/R-cran-data.table \
				R-cran-dplyr>0:math/R-cran-dplyr \
				R-cran-ggplot2>0:graphics/R-cran-ggplot2 \
				R-cran-R.utils>0:devel/R-cran-R.utils

EXAMPLE_SCRIPTS_CMAKE_BOOL=	WITH_EXAMPLE_SCRIPTS
EXAMPLE_RESULTS_CMAKE_BOOL=	WITH_EXAMPLE_RESULTS

# Work-around for build issue on ARM:
# https://bugs.freebsd.org/285706 https://bugs.freebsd.org/292237
MAKE_ENV=	OMP_NUM_THREADS=1

.include <bsd.port.mk>
