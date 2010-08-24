# New ports collection makefile for: netperfmeter
# Date created:                      24 August 2010
# Whom:                              Thomas Dreibholz <dreibh@iem.uni-due.de>
#
# $FreeBSD$
#

PORTNAME=	netperfmeter
PORTVERSION=	1.0.4
CATEGORIES=	benchmarks
MASTER_SITES=	http://www.iem.uni-due.de/~dreibh/netperfmeter/download/ \
		http://www.exp-math.uni-essen.de/~dreibh/netperfmeter/download/
DISTNAME=	${PORTNAME}-${PORTVERSION}

MAINTAINER=     dreibh@iem.uni-due.de
COMMENT=        Network Performance Meter

.if !exists(/usr/bin/bzip2)
RUN_DEPENDS+=   bzip2:${PORTSDIR}/archivers/bzip2
.endif
.if !exists(/usr/lib/libbz2.so)
LIB_DEPENDS+=   bz2:${PORTSDIR}/archivers/bzip2
.endif

GNU_CONFIGURE=  yes
USE_GMAKE=      yes

LICENSE=        GPLv3
LICENSE_FILE=   ${WRKSRC}/COPYING

.include <bsd.port.mk>
