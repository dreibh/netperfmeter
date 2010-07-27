# New ports collection makefile for: netperfmeter
# Date created:                      27 July 2010
# Whom:                              Thomas Dreibholz <dreibh@iem.uni-due.de>
#
# $FreeBSD$
#

PORTNAME=	netperfmeter
PORTVERSION=	1.0.4
CATEGORIES=	net
MASTER_SITES=	http://www.iem.uni-due.de/~dreibh/netperfmeter/download/ http://www.exp-math.uni-essen.de/~dreibh/netperfmeter/download/
DISTNAME=	${PORTNAME}-${PORTVERSION}

MAINTAINER=     dreibh@iem.uni-due.de
COMMENT=        Network Performance Meter

RUN_DEPENDS=    bzip2:${PORTSDIR}/archivers/bzip2
LIB_DEPENDS=    bz2.1:${PORTSDIR}/archivers/bzip2

GNU_CONFIGURE=  yes
USE_GMAKE=      yes

LICENSE=        GPLv3
LICENSE_FILE=   ${WRKSRC}/COPYING

pre-fetch:

post-patch:

pre-install:

.include <bsd.port.mk>
