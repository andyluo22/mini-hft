FROM node:20-alpine
WORKDIR /app
ENV NEXT_TELEMETRY_DISABLED=1
COPY package.json ./
RUN npm install --no-audit --no-fund
COPY . .
ENV NEXT_PUBLIC_API_BASE=http://api:8000
RUN npm run build
EXPOSE 3000
CMD ["npm", "run", "start"]