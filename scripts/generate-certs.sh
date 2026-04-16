#!/bin/bash
# OmniNet TLS/SSL Certificate Generator
# Creates self-signed certificates for local development

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Generate TLS/SSL certificates for OmniNet development"
    echo ""
    echo "Options:"
    echo "  -d, --dir <path>     Output directory (default: certificates/)"
    echo "  -n, --name <name>    Certificate name prefix (default: omninet)"
    echo "  -h, --host <host>   Server hostname (default: localhost)"
    echo "  -e, --email <email> Email for certificate (default: admin@localhost)"
    echo "  --days <days>        Certificate validity in days (default: 365)"
    echo "  --force              Overwrite existing certificates"
    echo "  -q, --quiet          Suppress output"
    echo "  -h, --help           Show this help message"
    echo ""
    echo "Example:"
    echo "  $0 --dir ~/.omninet/certificates --host localhost"
    exit 0
}

# Default values
OUTPUT_DIR="certificates"
CERT_NAME="omninet"
HOSTNAME="localhost"
EMAIL="admin@localhost"
DAYS=365
FORCE=false
QUIET=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--dir)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        -n|--name)
            CERT_NAME="$2"
            shift 2
            ;;
        -h|--host)
            HOSTNAME="$2"
            shift 2
            ;;
        -e|--email)
            EMAIL="$2"
            shift 2
            ;;
        --days)
            DAYS="$2"
            shift 2
            ;;
        --force)
            FORCE=true
            shift
            ;;
        -q|--quiet)
            QUIET=true
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "Unknown option: $1"
            usage
            ;;
    esac
done

# Helper functions
log() {
    if [[ "$QUIET" == false ]]; then
        echo -e "$1"
    fi
}

error() {
    echo -e "${RED}Error: $1${NC}" >&2
    exit 1
}

success() {
    echo -e "${GREEN}✓ $1${NC}"
}

warn() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Define file paths
CERT_FILE="${OUTPUT_DIR}/${CERT_NAME}.crt"
KEY_FILE="${OUTPUT_DIR}/${CERT_NAME}.key"
CSR_FILE="${OUTPUT_DIR}/${CERT_NAME}.csr"
PEM_FILE="${OUTPUT_DIR}/${CERT_NAME}.pem"
CONF_FILE="${OUTPUT_DIR}/${CERT_NAME}.cnf"

# Check for existing files
if [[ -f "$CERT_FILE" ]] || [[ -f "$KEY_FILE" ]]; then
    if [[ "$FORCE" == true ]]; then
        warn "Overwriting existing certificates..."
        rm -f "$CERT_FILE" "$KEY_FILE" "$CSR_FILE" "$PEM_FILE" "$CONF_FILE"
    else
        error "Certificates already exist in $OUTPUT_DIR. Use --force to overwrite."
    fi
fi

# Check if OpenSSL is available
if ! command -v openssl &> /dev/null; then
    error "OpenSSL is not installed. Please install it: sudo apt install openssl (Debian/Ubuntu) or sudo yum install openssl (CentOS/RHEL)"
fi

log "Generating TLS certificates..."
log "  Output: $OUTPUT_DIR/"
log "  Certificate: $CERT_FILE"
log "  Private Key: $KEY_FILE"
log "  Validity: $DAYS days"
log ""

# Generate OpenSSL config
cat > "$CONF_FILE" << EOF
[req]
default_bits = 2048
prompt = no
default_md = sha256
distinguished_name = dn
x509_extensions = v3_req

[dn]
C = US
ST = State
L = City
O = OmniNet
OU = Development
CN = $HOSTNAME
emailAddress = $EMAIL

[v3_req]
subjectAltName = @alt_names

[alt_names]
DNS.1 = localhost
DNS.2 = $HOSTNAME
DNS.3 = *.localhost
IP.1 = 127.0.0.1
IP.2 = ::1
EOF

log "Creating private key and certificate..."

# Generate private key and self-signed certificate in one command
openssl req -x509 \
    -newkey rsa:4096 \
    -keyout "$KEY_FILE" \
    -out "$CERT_FILE" \
    -days "$DAYS" \
    -nodes \
    -config "$CONF_FILE" \
    -subj "/C=US/ST=State/L=City/O=OmniNet/OU=Development/CN=$HOSTNAME/emailAddress=$EMAIL" \
    2>/dev/null

if [[ $? -ne 0 ]]; then
    error "Failed to generate certificate"
fi

# Create combined PEM file (cert + key) for some servers
cat "$CERT_FILE" "$KEY_FILE" > "$PEM_FILE"

# Set proper permissions
chmod 600 "$KEY_FILE"
chmod 644 "$CERT_FILE"
chmod 644 "$PEM_FILE"

log ""
success "Certificates generated successfully!"
log ""
echo "Files created:"
echo "  ${GREEN}$CERT_FILE${NC}  - Certificate (public)"
echo "  ${GREEN}$KEY_FILE${NC}   - Private key (keep secret!)"
echo "  ${GREEN}$PEM_FILE${NC}   - Combined cert+key"
echo "  ${GREEN}$CONF_FILE${NC}  - OpenSSL config"
echo ""

# Verify the certificate
log "Certificate details:"
echo ""
openssl x509 -in "$CERT_FILE" -noout -text 2>/dev/null | head -20

echo ""
log "To use with OmniNet, add to config.json:"
echo ""
echo '  "tls": {'
echo '    "enabled": true,'
echo "    \"certFile\": \"$CERT_FILE\","
echo "    \"keyFile\": \"$KEY_FILE\","
echo '    "verifyClient": false'
echo '  }'
echo ""

# For security warning
warn "⚠️  This is a self-signed certificate for development only!"
warn "    For production, use certificates from a trusted CA (Let's Encrypt, etc.)"