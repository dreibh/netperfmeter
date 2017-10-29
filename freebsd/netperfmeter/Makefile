# New ports collection makefile for: netperfmeter
# Date created:                      27 July 2010
# Whom:                              Thomas Dreibholz <dreibh@iem.uni-due.de>
#
# $FreeBSD: ports/benchmarks/netperfmeter/Makefile,v 1.2 2010/12/29 20:04:12 pgollucci Exp $
#

PORTNAME=	netperfmeter
PORTVERSION=	1.7.6
CATEGORIES=	benchmarks
MASTER_SITES=	https://www.uni-due.de/~be0001/netperfmeter/download/

MAINTAINER=	dreibh@iem.uni-due.de
COMMENT=	Network Performance Meter

USES=           cmake shebangfix
SHEBANG_FILES=  src/pdfmetadata
PLIST_FILES=	bin/netperfmeter
MAN1=		netperfmeter.1

LICENSE=	GPLv3
LICENSE_FILE=	${WRKSRC}/COPYING

MAKE_JOBS_SAFE=	yes

.include <bsd.port.pre.mk>

.if ${OSVERSION} < 700000
IGNORE=	not supported on systems earlier than 7.x
.endif

.include <bsd.port.post.mk>
